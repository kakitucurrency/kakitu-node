#include <nano/lib/logger_mt.hpp>
#include <nano/lib/stats.hpp>
#include <nano/lib/threading.hpp>
#include <nano/lib/timer.hpp>
#include <nano/node/active_transactions.hpp>
#include <nano/node/node_observers.hpp>
#include <nano/node/nodeconfig.hpp>
#include <nano/node/online_reps.hpp>
#include <nano/node/repcrawler.hpp>
#include <nano/node/signatures.hpp>
#include <nano/node/vote_processor.hpp>
#include <nano/secure/common.hpp>
#include <nano/secure/ledger.hpp>

#include <boost/format.hpp>

#include <chrono>
using namespace std::chrono_literals;

nano::vote_processor::vote_processor (nano::signature_checker & checker_a, nano::active_transactions & active_a, nano::node_observers & observers_a, nano::stats & stats_a, nano::node_config & config_a, nano::node_flags & flags_a, nano::logger_mt & logger_a, nano::online_reps & online_reps_a, nano::rep_crawler & rep_crawler_a, nano::ledger & ledger_a, nano::network_params & network_params_a) :
	checker (checker_a),
	active (active_a),
	observers (observers_a),
	stats (stats_a),
	config (config_a),
	logger (logger_a),
	online_reps (online_reps_a),
	rep_crawler (rep_crawler_a),
	ledger (ledger_a),
	network_params (network_params_a),
	max_votes (flags_a.vote_processor_capacity),
	started (false),
	stopped (false)
{
	for (unsigned i = 0; i < num_shards; ++i)
	{
		threads[i] = std::thread ([this, i] () {
			nano::thread_role::set (nano::thread_role::name::vote_processing);
			process_loop (i);
			nano::unique_lock<nano::mutex> lock{ shard_mutexes[i] };
			shard_votes[i].clear ();
			shard_conditions[i].notify_all ();
		});
	}
	nano::unique_lock<nano::mutex> lock{ mutex };
	condition.wait (lock, [&started = started] { return started; });
}

unsigned nano::vote_processor::shard_index (nano::account const & account) const
{
	return account.bytes[0] % num_shards;
}

void nano::vote_processor::process_loop (unsigned shard)
{
	nano::timer<std::chrono::milliseconds> elapsed;
	bool log_this_iteration;

	// Signal started from first shard only
	if (shard == 0)
	{
		nano::unique_lock<nano::mutex> lock{ mutex };
		started = true;
		lock.unlock ();
		condition.notify_all ();
	}

	nano::unique_lock<nano::mutex> lock{ shard_mutexes[shard] };

	while (!stopped)
	{
		if (!shard_votes[shard].empty ())
		{
			std::deque<std::pair<std::shared_ptr<nano::vote>, std::shared_ptr<nano::transport::channel>>> votes_l;
			votes_l.swap (shard_votes[shard]);
			lock.unlock ();
			shard_conditions[shard].notify_all ();

			log_this_iteration = false;
			if (config.logging.network_logging () && votes_l.size () > 50)
			{
				log_this_iteration = true;
				elapsed.restart ();
			}
			verify_votes (votes_l);
			total_processed += votes_l.size ();

			if (log_this_iteration && elapsed.stop () > std::chrono::milliseconds (100))
			{
				logger.try_log (boost::str (boost::format ("Shard %1%: Processed %2% votes in %3% milliseconds (rate of %4% votes per second)") % shard % votes_l.size () % elapsed.value ().count () % ((votes_l.size () * 1000ULL) / elapsed.value ().count ())));
			}
			lock.lock ();
		}
		else
		{
			shard_conditions[shard].wait (lock);
		}
	}
}

bool nano::vote_processor::vote (std::shared_ptr<nano::vote> const & vote_a, std::shared_ptr<nano::transport::channel> const & channel_a)
{
	debug_assert (channel_a != nullptr);
	bool process (false);
	auto shard = shard_index (vote_a->account);
	nano::unique_lock<nano::mutex> lock{ shard_mutexes[shard] };
	if (!stopped)
	{
		auto const shard_max = max_votes / num_shards;
		// Level 0 (< 0.1%)
		if (shard_votes[shard].size () < 6.0 / 9.0 * shard_max)
		{
			process = true;
		}
		// Level 1 (0.1-1%)
		else if (shard_votes[shard].size () < 7.0 / 9.0 * shard_max)
		{
			process = (representatives_1.find (vote_a->account) != representatives_1.end ());
		}
		// Level 2 (1-5%)
		else if (shard_votes[shard].size () < 8.0 / 9.0 * shard_max)
		{
			process = (representatives_2.find (vote_a->account) != representatives_2.end ());
		}
		// Level 3 (> 5%)
		else if (shard_votes[shard].size () < shard_max)
		{
			process = (representatives_3.find (vote_a->account) != representatives_3.end ());
		}
		if (process)
		{
			shard_votes[shard].emplace_back (vote_a, channel_a);
			lock.unlock ();
			shard_conditions[shard].notify_all ();
		}
		else
		{
			stats.inc (nano::stat::type::vote, nano::stat::detail::vote_overflow);
		}
	}
	return !process;
}

void nano::vote_processor::verify_votes (std::deque<vote_entry> const & votes_a)
{
	auto size (votes_a.size ());
	std::vector<unsigned char const *> messages;
	messages.reserve (size);
	std::vector<nano::block_hash> hashes;
	hashes.reserve (size);
	std::vector<std::size_t> lengths (size, sizeof (nano::block_hash));
	std::vector<unsigned char const *> pub_keys;
	pub_keys.reserve (size);
	std::vector<unsigned char const *> signatures;
	signatures.reserve (size);
	std::vector<int> verifications;
	verifications.resize (size);
	for (auto const & vote : votes_a)
	{
		hashes.push_back (vote.first->hash ());
		messages.push_back (hashes.back ().bytes.data ());
		pub_keys.push_back (vote.first->account.bytes.data ());
		signatures.push_back (vote.first->signature.bytes.data ());
	}
	nano::signature_check_set check = { size, messages.data (), lengths.data (), pub_keys.data (), signatures.data (), verifications.data () };
	checker.verify (check);
	auto i (0);
	for (auto const & vote : votes_a)
	{
		debug_assert (verifications[i] == 1 || verifications[i] == 0);
		if (verifications[i] == 1)
		{
			vote_blocking (vote.first, vote.second, true);
		}
		++i;
	}
}

nano::vote_code nano::vote_processor::vote_blocking (std::shared_ptr<nano::vote> const & vote_a, std::shared_ptr<nano::transport::channel> const & channel_a, bool validated)
{
	auto result (nano::vote_code::invalid);
	if (validated || !vote_a->validate ())
	{
		result = active.vote (vote_a);
		observers.vote.notify (vote_a, channel_a, result);
	}
	std::string status;
	switch (result)
	{
		case nano::vote_code::invalid:
			status = "Invalid";
			stats.inc (nano::stat::type::vote, nano::stat::detail::vote_invalid);
			break;
		case nano::vote_code::replay:
			status = "Replay";
			stats.inc (nano::stat::type::vote, nano::stat::detail::vote_replay);
			break;
		case nano::vote_code::vote:
			status = "Vote";
			stats.inc (nano::stat::type::vote, nano::stat::detail::vote_valid);
			break;
		case nano::vote_code::indeterminate:
			status = "Indeterminate";
			stats.inc (nano::stat::type::vote, nano::stat::detail::vote_indeterminate);
			break;
	}
	if (config.logging.vote_logging ())
	{
		logger.try_log (boost::str (boost::format ("Vote from: %1% timestamp: %2% duration %3%ms block(s): %4% status: %5%") % vote_a->account.to_account () % std::to_string (vote_a->timestamp ()) % std::to_string (vote_a->duration ().count ()) % vote_a->hashes_string () % status));
	}
	return result;
}

void nano::vote_processor::stop ()
{
	{
		nano::lock_guard<nano::mutex> lock{ mutex };
		stopped = true;
	}
	for (unsigned i = 0; i < num_shards; ++i)
	{
		shard_conditions[i].notify_all ();
	}
	condition.notify_all ();
	for (unsigned i = 0; i < num_shards; ++i)
	{
		if (threads[i].joinable ())
		{
			threads[i].join ();
		}
	}
}

void nano::vote_processor::flush ()
{
	auto const cutoff = total_processed.load (std::memory_order_relaxed) + size ();
	for (unsigned i = 0; i < num_shards; ++i)
	{
		nano::unique_lock<nano::mutex> lock{ shard_mutexes[i] };
		bool success = shard_conditions[i].wait_for (lock, 60s, [this, &cutoff, i] () {
			return stopped || shard_votes[i].empty () || total_processed.load (std::memory_order_relaxed) >= cutoff;
		});
		if (!success)
		{
			logger.always_log ("WARNING: vote_processor::flush timeout while waiting for flush");
			debug_assert (false && "vote_processor::flush timeout while waiting for flush");
		}
	}
}

std::size_t nano::vote_processor::size ()
{
	std::size_t total = 0;
	for (unsigned i = 0; i < num_shards; ++i)
	{
		nano::lock_guard<nano::mutex> guard{ shard_mutexes[i] };
		total += shard_votes[i].size ();
	}
	return total;
}

bool nano::vote_processor::empty ()
{
	for (unsigned i = 0; i < num_shards; ++i)
	{
		nano::lock_guard<nano::mutex> guard{ shard_mutexes[i] };
		if (!shard_votes[i].empty ())
		{
			return false;
		}
	}
	return true;
}

bool nano::vote_processor::half_full ()
{
	return size () >= max_votes / 2;
}

void nano::vote_processor::calculate_weights ()
{
	nano::unique_lock<nano::mutex> lock{ mutex };
	if (!stopped)
	{
		representatives_1.clear ();
		representatives_2.clear ();
		representatives_3.clear ();
		auto supply (online_reps.trended ());
		auto rep_amounts = ledger.cache.rep_weights.get_rep_amounts ();
		for (auto const & rep_amount : rep_amounts)
		{
			nano::account const & representative (rep_amount.first);
			auto weight (ledger.weight (representative));
			if (weight > supply / 1000) // 0.1% or above (level 1)
			{
				representatives_1.insert (representative);
				if (weight > supply / 100) // 1% or above (level 2)
				{
					representatives_2.insert (representative);
					if (weight > supply / 20) // 5% or above (level 3)
					{
						representatives_3.insert (representative);
					}
				}
			}
		}
	}
}

std::unique_ptr<nano::container_info_component> nano::collect_container_info (vote_processor & vote_processor, std::string const & name)
{
	std::size_t votes_count = 0;
	std::size_t representatives_1_count;
	std::size_t representatives_2_count;
	std::size_t representatives_3_count;

	for (unsigned i = 0; i < nano::vote_processor::num_shards; ++i)
	{
		nano::lock_guard<nano::mutex> guard{ vote_processor.shard_mutexes[i] };
		votes_count += vote_processor.shard_votes[i].size ();
	}
	{
		nano::lock_guard<nano::mutex> guard{ vote_processor.mutex };
		representatives_1_count = vote_processor.representatives_1.size ();
		representatives_2_count = vote_processor.representatives_2.size ();
		representatives_3_count = vote_processor.representatives_3.size ();
	}

	auto composite = std::make_unique<container_info_composite> (name);
	using vote_pair = std::pair<std::shared_ptr<nano::vote>, std::shared_ptr<nano::transport::channel>>;
	composite->add_component (std::make_unique<container_info_leaf> (container_info{ "votes", votes_count, sizeof (vote_pair) }));
	composite->add_component (std::make_unique<container_info_leaf> (container_info{ "representatives_1", representatives_1_count, sizeof (decltype (vote_processor.representatives_1)::value_type) }));
	composite->add_component (std::make_unique<container_info_leaf> (container_info{ "representatives_2", representatives_2_count, sizeof (decltype (vote_processor.representatives_2)::value_type) }));
	composite->add_component (std::make_unique<container_info_leaf> (container_info{ "representatives_3", representatives_3_count, sizeof (decltype (vote_processor.representatives_3)::value_type) }));
	return composite;
}
