#include <nano/crypto_lib/random_pool.hpp>
#include <nano/lib/config.hpp>
#include <nano/lib/numbers.hpp>
#include <nano/lib/timer.hpp>
#include <nano/secure/common.hpp>
#include <nano/secure/store.hpp>

#include <boost/endian/conversion.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/variant/get.hpp>

#include <limits>
#include <queue>

#include <crypto/ed25519-donna/ed25519.h>
#include <cryptopp/words.h>

size_t constexpr nano::send_block::size;
size_t constexpr nano::receive_block::size;
size_t constexpr nano::open_block::size;
size_t constexpr nano::change_block::size;
size_t constexpr nano::state_block::size;

nano::networks nano::network_constants::active_network = nano::networks::ACTIVE_NETWORK;

namespace
{
char const * dev_private_key_data = "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4";
char const * dev_public_key_data = "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // xrb_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo
char const * beta_public_key_data = "259A43ABDB779E97452E188BA3EB951B41C961D3318CA6B925380F4D99F0577A"; // kshs_1betagoxpxwykx4kw86dnhosc8t3s7ix8eeentwkcg1hbpez1outjrcyg4n1
char const * live_public_key_data = "E89208DD038FBB269987689621D52292AE9C35941A7484756ECCED92A65093BA"; // xrb_3t6k35gi95xu6tergt6p69ck76ogmitsa8mnijtpxm9fkcm736xtoncuohr3
std::string const test_public_key_data = nano::get_env_or_default ("NANO_TEST_GENESIS_PUB", "45C6FF9D1706D61F0821327752671BDA9F9ED2DA40326B01935AB566FB9E08ED"); // kshs_1jg8zygjg3pp5w644emqcbmjqpnzmubfni3kfe1s8pooeuxsw49fdq1mco9j
char const * dev_genesis_data = R"%%%({
	"type": "open",
	"source": "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
	"representative": "xrb_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
	"account": "xrb_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
	"work": "7b42a00ee91d5810",
	"signature": "ECDA914373A2F0CA1296475BAEE40500A7F0A7AD72A5A80C81D7FAB7F6C802B2CC7DB50F5DD0FB25B2EF11761FA7344A158DD5A700B21BD47DE5BD0F63153A02"
    })%%%";

char const * beta_genesis_data = R"%%%({
	"type": "open",
	"source": "259A43ABDB779E97452E188BA3EB951B41C961D3318CA6B925380F4D99F0577A",
	"representative": "kshs_1betagoxpxwykx4kw86dnhosc8t3s7ix8eeentwkcg1hbpez1outjrcyg4n1",
	"account": "kshs_1betagoxpxwykx4kw86dnhosc8t3s7ix8eeentwkcg1hbpez1outjrcyg4n1",
	"work": "79d4e27dc873c6f2",
	"signature": "4BD7F96F9ED2721BCEE5EAED400EA50AD00524C629AE55E9AFF11220D2C1B00C3D4B3BB770BF67D4F8658023B677F91110193B6C101C2666931F57046A6DB806"
    })%%%";

char const * live_genesis_data = R"%%%({
	"type": "open",
	"source": "E89208DD038FBB269987689621D52292AE9C35941A7484756ECCED92A65093BA",
	"representative": "xrb_3t6k35gi95xu6tergt6p69ck76ogmitsa8mnijtpxm9fkcm736xtoncuohr3",
	"account": "xrb_3t6k35gi95xu6tergt6p69ck76ogmitsa8mnijtpxm9fkcm736xtoncuohr3",
	"work": "62f05417dd3fb691",
	"signature": "9F0C933C8ADE004D808EA1985FA746A7E95BA2A38F867640F53EC8F180BDFE9E2C1268DEAD7C2664F356E37ABA362BC58E46DBA03E523A7B5A19E4B6EB12BB02"
    })%%%";

std::string const test_genesis_data = nano::get_env_or_default ("NANO_TEST_GENESIS_BLOCK", R"%%%({
	"type": "open",
	"source": "45C6FF9D1706D61F0821327752671BDA9F9ED2DA40326B01935AB566FB9E08ED",
	"representative": "kshs_1jg8zygjg3pp5w644emqcbmjqpnzmubfni3kfe1s8pooeuxsw49fdq1mco9j",
	"account": "kshs_1jg8zygjg3pp5w644emqcbmjqpnzmubfni3kfe1s8pooeuxsw49fdq1mco9j",
	"work": "bc1ef279c1a34eb1",
	"signature": "15049467CAEE3EC768639E8E35792399B6078DA763DA4EBA8ECAD33B0EDC4AF2E7403893A5A602EB89B978DABEF1D6606BB00F3C0EE11449232B143B6E07170E"
    })%%%");

std::shared_ptr<nano::block> parse_block_from_genesis_data (std::string const & genesis_data_a)
{
	boost::property_tree::ptree tree;
	std::stringstream istream (genesis_data_a);
	boost::property_tree::read_json (istream, tree);
	return nano::deserialize_block_json (tree);
}

char const * beta_canary_public_key_data = "868C6A9F79D4506E029B378262B91538C5CB26D7C346B63902FFEB365F1C1947"; // kshs_33nefchqmo4ifr3bpfw4ecwjcg87semfhit8prwi7zzd8shjr8c9qdxeqmnx
char const * live_canary_public_key_data = "7CBAF192A3763DAEC9F9BAC1B2CDF665D8369F8400B4BC5AB4BA31C00BAA4404"; // kshs_1z7ty8bc8xjxou6zmgp3pd8zesgr8thra17nqjfdbgjjr17tnj16fjntfqfn
std::string const test_canary_public_key_data = nano::get_env_or_default ("NANO_TEST_CANARY_PUB", "3BAD2C554ACE05F5E528FBBCE79D51E552C55FA765CCFD89B289C4835DE5F04A"); // kshs_1gxf7jcnomi7yqkkjyxwwygo5sckrohtgsgezp6u74g6ifgydw4cajwbk8bf
}

nano::keypair nano::dev::genesis_key{ dev_private_key_data };
nano::network_params nano::dev::network_params{ nano::networks::kshs_dev_network };
nano::ledger_constants & nano::dev::constants{ nano::dev::network_params.ledger };
std::shared_ptr<nano::block> & nano::dev::genesis = nano::dev::constants.genesis;

nano::network_params::network_params (nano::networks network_a) :
	work{ network_a == nano::networks::kshs_live_network ? nano::work_thresholds::publish_full : network_a == nano::networks::kshs_beta_network ? nano::work_thresholds::publish_beta
		: network_a == nano::networks::kshs_test_network                                                                                        ? nano::work_thresholds::publish_test
																																				: nano::work_thresholds::publish_dev },
	network{ work, network_a },
	ledger{ work, network_a },
	voting{ network },
	node{ network },
	portmapping{ network },
	bootstrap{ network }
{
	unsigned constexpr kdf_full_work = 64 * 1024;
	unsigned constexpr kdf_dev_work = 8;
	kdf_work = network.is_dev_network () ? kdf_dev_work : kdf_full_work;
}

nano::ledger_constants::ledger_constants (nano::work_thresholds & work, nano::networks network_a) :
	work{ work },
	zero_key ("0"),
	kshs_beta_account (beta_public_key_data),
	kshs_live_account (live_public_key_data),
	kshs_test_account (test_public_key_data),
	kshs_dev_genesis (parse_block_from_genesis_data (dev_genesis_data)),
	kshs_beta_genesis (parse_block_from_genesis_data (beta_genesis_data)),
	kshs_live_genesis (parse_block_from_genesis_data (live_genesis_data)),
	kshs_test_genesis (parse_block_from_genesis_data (test_genesis_data)),
	genesis (network_a == nano::networks::kshs_dev_network ? kshs_dev_genesis : network_a == nano::networks::kshs_beta_network ? kshs_beta_genesis
	: network_a == nano::networks::kshs_test_network                                                                           ? kshs_test_genesis
																															   : kshs_live_genesis),
	genesis_amount{ std::numeric_limits<nano::uint128_t>::max () },
	burn_account{},
	kshs_dev_final_votes_canary_account (dev_public_key_data),
	kshs_beta_final_votes_canary_account (beta_canary_public_key_data),
	kshs_live_final_votes_canary_account (live_canary_public_key_data),
	kshs_test_final_votes_canary_account (test_canary_public_key_data),
	final_votes_canary_account (network_a == nano::networks::kshs_dev_network ? kshs_dev_final_votes_canary_account : network_a == nano::networks::kshs_beta_network ? kshs_beta_final_votes_canary_account
	: network_a == nano::networks::kshs_test_network                                                                                                                 ? kshs_test_final_votes_canary_account
																																									 : kshs_live_final_votes_canary_account),
	kshs_dev_final_votes_canary_height (1),
	kshs_beta_final_votes_canary_height (1),
	kshs_live_final_votes_canary_height (1),
	kshs_test_final_votes_canary_height (1),
	final_votes_canary_height (network_a == nano::networks::kshs_dev_network ? kshs_dev_final_votes_canary_height : network_a == nano::networks::kshs_beta_network ? kshs_beta_final_votes_canary_height
	: network_a == nano::networks::kshs_test_network                                                                                                               ? kshs_test_final_votes_canary_height
																																								   : kshs_live_final_votes_canary_height)
{
	kshs_beta_genesis->sideband_set (nano::block_sideband (kshs_beta_genesis->account (), 0, std::numeric_limits<nano::uint128_t>::max (), 1, nano::seconds_since_epoch (), nano::epoch::epoch_0, false, false, false, nano::epoch::epoch_0));
	kshs_dev_genesis->sideband_set (nano::block_sideband (kshs_dev_genesis->account (), 0, std::numeric_limits<nano::uint128_t>::max (), 1, nano::seconds_since_epoch (), nano::epoch::epoch_0, false, false, false, nano::epoch::epoch_0));
	kshs_live_genesis->sideband_set (nano::block_sideband (kshs_live_genesis->account (), 0, std::numeric_limits<nano::uint128_t>::max (), 1, nano::seconds_since_epoch (), nano::epoch::epoch_0, false, false, false, nano::epoch::epoch_0));
	kshs_test_genesis->sideband_set (nano::block_sideband (kshs_test_genesis->account (), 0, std::numeric_limits<nano::uint128_t>::max (), 1, nano::seconds_since_epoch (), nano::epoch::epoch_0, false, false, false, nano::epoch::epoch_0));

	nano::link epoch_link_v1;
	char const * epoch_message_v1 ("epoch v1 block");
	strncpy ((char *)epoch_link_v1.bytes.data (), epoch_message_v1, epoch_link_v1.bytes.size ());
	epochs.add (nano::epoch::epoch_1, genesis->account (), epoch_link_v1);

	nano::link epoch_link_v2;
	nano::account kshs_live_epoch_v2_signer;
	auto error (kshs_live_epoch_v2_signer.decode_account ("kshs_3qb6o6i1tkzr6jwr5s7eehfxwg9x6eemitdinbpi7u8bjjwsgqfj4wzser3x"));
	debug_assert (!error);
	auto epoch_v2_signer (network_a == nano::networks::kshs_dev_network ? nano::dev::genesis_key.pub : network_a == nano::networks::kshs_beta_network ? kshs_beta_account
	: network_a == nano::networks::kshs_test_network                                                                                                  ? kshs_test_account
																																					  : kshs_live_epoch_v2_signer);
	char const * epoch_message_v2 ("epoch v2 block");
	strncpy ((char *)epoch_link_v2.bytes.data (), epoch_message_v2, epoch_link_v2.bytes.size ());
	epochs.add (nano::epoch::epoch_2, epoch_v2_signer, epoch_link_v2);
}

nano::hardened_constants & nano::hardened_constants::get ()
{
	static hardened_constants instance{};
	return instance;
}

nano::hardened_constants::hardened_constants () :
	not_an_account{},
	random_128{}
{
	nano::random_pool::generate_block (not_an_account.bytes.data (), not_an_account.bytes.size ());
	nano::random_pool::generate_block (random_128.bytes.data (), random_128.bytes.size ());
}

nano::node_constants::node_constants (nano::network_constants & network_constants)
{
	backup_interval = std::chrono::minutes (5);
	search_pending_interval = network_constants.is_dev_network () ? std::chrono::seconds (1) : std::chrono::seconds (5 * 60);
	unchecked_cleaning_interval = std::chrono::minutes (30);
	process_confirmed_interval = network_constants.is_dev_network () ? std::chrono::milliseconds (50) : std::chrono::milliseconds (500);
	max_weight_samples = (network_constants.is_live_network () || network_constants.is_test_network ()) ? 4032 : 288;
	weight_period = 5 * 60; // 5 minutes
}

nano::voting_constants::voting_constants (nano::network_constants & network_constants) :
	max_cache{ network_constants.is_dev_network () ? 256U : 128U * 1024 },
	delay{ network_constants.is_dev_network () ? 1 : 15 }
{
}

nano::portmapping_constants::portmapping_constants (nano::network_constants & network_constants)
{
	lease_duration = std::chrono::seconds (1787); // ~30 minutes
	health_check_period = std::chrono::seconds (53);
}

nano::bootstrap_constants::bootstrap_constants (nano::network_constants & network_constants)
{
	lazy_max_pull_blocks = network_constants.is_dev_network () ? 2 : 512;
	lazy_min_pull_blocks = network_constants.is_dev_network () ? 1 : 32;
	frontier_retry_limit = network_constants.is_dev_network () ? 2 : 16;
	lazy_retry_limit = network_constants.is_dev_network () ? 2 : frontier_retry_limit * 4;
	lazy_destinations_retry_limit = network_constants.is_dev_network () ? 1 : frontier_retry_limit / 4;
	gap_cache_bootstrap_start_interval = network_constants.is_dev_network () ? std::chrono::milliseconds (5) : std::chrono::milliseconds (30 * 1000);
	default_frontiers_age_seconds = network_constants.is_dev_network () ? 1 : 24 * 60 * 60; // 1 second for dev network, 24 hours for live/beta
}

// Create a new random keypair
nano::keypair::keypair ()
{
	random_pool::generate_block (prv.bytes.data (), prv.bytes.size ());
	ed25519_publickey (prv.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a private key
nano::keypair::keypair (nano::raw_key && prv_a) :
	prv (std::move (prv_a))
{
	ed25519_publickey (prv.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a hex string of the private key
nano::keypair::keypair (std::string const & prv_a)
{
	[[maybe_unused]] auto error (prv.decode_hex (prv_a));
	debug_assert (!error);
	ed25519_publickey (prv.bytes.data (), pub.bytes.data ());
}

nano::account_info::account_info (nano::block_hash const & head_a, nano::account const & representative_a, nano::block_hash const & open_block_a, nano::amount const & balance_a, nano::seconds_t modified_a, uint64_t block_count_a, nano::epoch epoch_a) :
	head (head_a),
	representative (representative_a),
	open_block (open_block_a),
	balance (balance_a),
	modified (modified_a),
	block_count (block_count_a),
	epoch_m (epoch_a)
{
}

bool nano::account_info::deserialize (nano::stream & stream_a)
{
	auto error (false);
	try
	{
		nano::read (stream_a, head.bytes);
		nano::read (stream_a, representative.bytes);
		nano::read (stream_a, open_block.bytes);
		nano::read (stream_a, balance.bytes);
		nano::read (stream_a, modified);
		nano::read (stream_a, block_count);
		nano::read (stream_a, epoch_m);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

bool nano::account_info::operator== (nano::account_info const & other_a) const
{
	return head == other_a.head && representative == other_a.representative && open_block == other_a.open_block && balance == other_a.balance && modified == other_a.modified && block_count == other_a.block_count && epoch () == other_a.epoch ();
}

bool nano::account_info::operator!= (nano::account_info const & other_a) const
{
	return !(*this == other_a);
}

size_t nano::account_info::db_size () const
{
	debug_assert (reinterpret_cast<uint8_t const *> (this) == reinterpret_cast<uint8_t const *> (&head));
	debug_assert (reinterpret_cast<uint8_t const *> (&head) + sizeof (head) == reinterpret_cast<uint8_t const *> (&representative));
	debug_assert (reinterpret_cast<uint8_t const *> (&representative) + sizeof (representative) == reinterpret_cast<uint8_t const *> (&open_block));
	debug_assert (reinterpret_cast<uint8_t const *> (&open_block) + sizeof (open_block) == reinterpret_cast<uint8_t const *> (&balance));
	debug_assert (reinterpret_cast<uint8_t const *> (&balance) + sizeof (balance) == reinterpret_cast<uint8_t const *> (&modified));
	debug_assert (reinterpret_cast<uint8_t const *> (&modified) + sizeof (modified) == reinterpret_cast<uint8_t const *> (&block_count));
	debug_assert (reinterpret_cast<uint8_t const *> (&block_count) + sizeof (block_count) == reinterpret_cast<uint8_t const *> (&epoch_m));
	return sizeof (head) + sizeof (representative) + sizeof (open_block) + sizeof (balance) + sizeof (modified) + sizeof (block_count) + sizeof (epoch_m);
}

nano::epoch nano::account_info::epoch () const
{
	return epoch_m;
}

nano::pending_info::pending_info (nano::account const & source_a, nano::amount const & amount_a, nano::epoch epoch_a) :
	source (source_a),
	amount (amount_a),
	epoch (epoch_a)
{
}

bool nano::pending_info::deserialize (nano::stream & stream_a)
{
	auto error (false);
	try
	{
		nano::read (stream_a, source.bytes);
		nano::read (stream_a, amount.bytes);
		nano::read (stream_a, epoch);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

size_t nano::pending_info::db_size () const
{
	return sizeof (source) + sizeof (amount) + sizeof (epoch);
}

bool nano::pending_info::operator== (nano::pending_info const & other_a) const
{
	return source == other_a.source && amount == other_a.amount && epoch == other_a.epoch;
}

nano::pending_key::pending_key (nano::account const & account_a, nano::block_hash const & hash_a) :
	account (account_a),
	hash (hash_a)
{
}

bool nano::pending_key::deserialize (nano::stream & stream_a)
{
	auto error (false);
	try
	{
		nano::read (stream_a, account.bytes);
		nano::read (stream_a, hash.bytes);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

bool nano::pending_key::operator== (nano::pending_key const & other_a) const
{
	return account == other_a.account && hash == other_a.hash;
}

nano::account const & nano::pending_key::key () const
{
	return account;
}

nano::unchecked_info::unchecked_info (std::shared_ptr<nano::block> const & block_a) :
	block (block_a),
	modified_m (nano::seconds_since_epoch ())
{
}

void nano::unchecked_info::serialize (nano::stream & stream_a) const
{
	debug_assert (block != nullptr);
	nano::serialize_block (stream_a, *block);
	nano::write (stream_a, modified_m);
}

bool nano::unchecked_info::deserialize (nano::stream & stream_a)
{
	block = nano::deserialize_block (stream_a);
	bool error (block == nullptr);
	if (!error)
	{
		try
		{
			nano::read (stream_a, modified_m);
		}
		catch (std::runtime_error const &)
		{
			error = true;
		}
	}
	return error;
}

uint64_t nano::unchecked_info::modified () const
{
	return modified_m;
}

nano::endpoint_key::endpoint_key (std::array<uint8_t, 16> const & address_a, uint16_t port_a) :
	address (address_a), network_port (boost::endian::native_to_big (port_a))
{
}

std::array<uint8_t, 16> const & nano::endpoint_key::address_bytes () const
{
	return address;
}

uint16_t nano::endpoint_key::port () const
{
	return boost::endian::big_to_native (network_port);
}

nano::confirmation_height_info::confirmation_height_info (uint64_t confirmation_height_a, nano::block_hash const & confirmed_frontier_a) :
	height (confirmation_height_a),
	frontier (confirmed_frontier_a)
{
}

void nano::confirmation_height_info::serialize (nano::stream & stream_a) const
{
	nano::write (stream_a, height);
	nano::write (stream_a, frontier);
}

bool nano::confirmation_height_info::deserialize (nano::stream & stream_a)
{
	auto error (false);
	try
	{
		nano::read (stream_a, height);
		nano::read (stream_a, frontier);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

nano::block_info::block_info (nano::account const & account_a, nano::amount const & balance_a) :
	account (account_a),
	balance (balance_a)
{
}

bool nano::vote::operator== (nano::vote const & other_a) const
{
	return timestamp_m == other_a.timestamp_m && hashes == other_a.hashes && account == other_a.account && signature == other_a.signature;
}

bool nano::vote::operator!= (nano::vote const & other_a) const
{
	return !(*this == other_a);
}

void nano::vote::serialize_json (boost::property_tree::ptree & tree) const
{
	tree.put ("account", account.to_account ());
	tree.put ("signature", signature.number ());
	tree.put ("sequence", std::to_string (timestamp ()));
	tree.put ("timestamp", std::to_string (timestamp ()));
	tree.put ("duration", std::to_string (duration_bits ()));
	boost::property_tree::ptree blocks_tree;
	for (auto const & hash : hashes)
	{
		boost::property_tree::ptree entry;
		entry.put ("", hash.to_string ());
		blocks_tree.push_back (std::make_pair ("", entry));
	}
	tree.add_child ("blocks", blocks_tree);
}

std::string nano::vote::to_json () const
{
	std::stringstream stream;
	boost::property_tree::ptree tree;
	serialize_json (tree);
	boost::property_tree::write_json (stream, tree);
	return stream.str ();
}

/**
 * Returns the timestamp of the vote (with the duration bits masked, set to zero)
 * If it is a final vote, all the bits including duration bits are returned as they are, all FF
 */
uint64_t nano::vote::timestamp () const
{
	return (timestamp_m == std::numeric_limits<uint64_t>::max ())
	? timestamp_m // final vote
	: (timestamp_m & timestamp_mask);
}

uint8_t nano::vote::duration_bits () const
{
	// Duration field is specified in the 4 low-order bits of the timestamp.
	// This makes the timestamp have a minimum granularity of 16ms
	// The duration is specified as 2^(duration + 4) giving it a range of 16-524,288ms in power of two increments
	auto result = timestamp_m & ~timestamp_mask;
	debug_assert (result < 16);
	return static_cast<uint8_t> (result);
}

std::chrono::milliseconds nano::vote::duration () const
{
	return std::chrono::milliseconds{ 1u << (duration_bits () + 4) };
}

nano::vote::vote (nano::vote const & other_a) :
	timestamp_m{ other_a.timestamp_m },
	hashes{ other_a.hashes },
	account (other_a.account),
	signature (other_a.signature)
{
}

nano::vote::vote (bool & error_a, nano::stream & stream_a)
{
	error_a = deserialize (stream_a);
}

nano::vote::vote (nano::account const & account_a, nano::raw_key const & prv_a, uint64_t timestamp_a, uint8_t duration, std::vector<nano::block_hash> const & hashes) :
	hashes{ hashes },
	timestamp_m{ packed_timestamp (timestamp_a, duration) },
	account (account_a)
{
	signature = nano::sign_message (prv_a, account_a, hash ());
}

std::string nano::vote::hashes_string () const
{
	std::string result;
	for (auto const & hash : hashes)
	{
		result += hash.to_string ();
		result += ", ";
	}
	return result;
}

std::string const nano::vote::hash_prefix = "vote ";

nano::block_hash nano::vote::hash () const
{
	nano::block_hash result;
	blake2b_state hash;
	blake2b_init (&hash, sizeof (result.bytes));
	blake2b_update (&hash, hash_prefix.data (), hash_prefix.size ());
	for (auto const & block_hash : hashes)
	{
		blake2b_update (&hash, block_hash.bytes.data (), sizeof (block_hash.bytes));
	}
	union
	{
		uint64_t qword;
		std::array<uint8_t, 8> bytes;
	};
	qword = timestamp_m;
	blake2b_update (&hash, bytes.data (), sizeof (bytes));
	blake2b_final (&hash, result.bytes.data (), sizeof (result.bytes));
	return result;
}

nano::block_hash nano::vote::full_hash () const
{
	nano::block_hash result;
	blake2b_state state;
	blake2b_init (&state, sizeof (result.bytes));
	blake2b_update (&state, hash ().bytes.data (), sizeof (hash ().bytes));
	blake2b_update (&state, account.bytes.data (), sizeof (account.bytes.data ()));
	blake2b_update (&state, signature.bytes.data (), sizeof (signature.bytes.data ()));
	blake2b_final (&state, result.bytes.data (), sizeof (result.bytes));
	return result;
}

void nano::vote::serialize (nano::stream & stream_a) const
{
	write (stream_a, account);
	write (stream_a, signature);
	write (stream_a, boost::endian::native_to_little (timestamp_m));
	for (auto const & hash : hashes)
	{
		write (stream_a, hash);
	}
}

bool nano::vote::deserialize (nano::stream & stream_a)
{
	auto error = false;
	try
	{
		nano::read (stream_a, account.bytes);
		nano::read (stream_a, signature.bytes);
		nano::read (stream_a, timestamp_m);

		while (stream_a.in_avail () > 0)
		{
			nano::block_hash block_hash;
			nano::read (stream_a, block_hash);
			hashes.push_back (block_hash);
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

bool nano::vote::validate () const
{
	return nano::validate_message (account, hash (), signature);
}

uint64_t nano::vote::packed_timestamp (uint64_t timestamp, uint8_t duration) const
{
	debug_assert (duration <= duration_max && "Invalid duration");
	debug_assert ((!(timestamp == timestamp_max) || (duration == duration_max)) && "Invalid final vote");
	return (timestamp & timestamp_mask) | duration;
}

nano::block_hash nano::iterate_vote_blocks_as_hash::operator() (nano::block_hash const & item) const
{
	return item;
}

nano::vote_uniquer::vote_uniquer (nano::block_uniquer & uniquer_a) :
	uniquer (uniquer_a)
{
}

std::shared_ptr<nano::vote> nano::vote_uniquer::unique (std::shared_ptr<nano::vote> const & vote_a)
{
	auto result = vote_a;
	if (result != nullptr)
	{
		nano::block_hash key = vote_a->full_hash ();
		nano::lock_guard<nano::mutex> lock{ mutex };
		auto & existing = votes[key];
		if (auto block_l = existing.lock ())
		{
			result = block_l;
		}
		else
		{
			existing = vote_a;
		}

		release_assert (std::numeric_limits<CryptoPP::word32>::max () > votes.size ());
		for (auto i (0); i < cleanup_count && !votes.empty (); ++i)
		{
			auto random_offset = nano::random_pool::generate_word32 (0, static_cast<CryptoPP::word32> (votes.size () - 1));

			auto existing (std::next (votes.begin (), random_offset));
			if (existing == votes.end ())
			{
				existing = votes.begin ();
			}
			if (existing != votes.end ())
			{
				if (auto block_l = existing->second.lock ())
				{
					// Still live
				}
				else
				{
					votes.erase (existing);
				}
			}
		}
	}
	return result;
}

size_t nano::vote_uniquer::size ()
{
	nano::lock_guard<nano::mutex> lock{ mutex };
	return votes.size ();
}

std::unique_ptr<nano::container_info_component> nano::collect_container_info (vote_uniquer & vote_uniquer, std::string const & name)
{
	auto count = vote_uniquer.size ();
	auto sizeof_element = sizeof (vote_uniquer::value_type);
	auto composite = std::make_unique<container_info_composite> (name);
	composite->add_component (std::make_unique<container_info_leaf> (container_info{ "votes", count, sizeof_element }));
	return composite;
}

nano::wallet_id nano::random_wallet_id ()
{
	nano::wallet_id wallet_id;
	nano::uint256_union dummy_secret;
	random_pool::generate_block (dummy_secret.bytes.data (), dummy_secret.bytes.size ());
	ed25519_publickey (dummy_secret.bytes.data (), wallet_id.bytes.data ());
	return wallet_id;
}

nano::unchecked_key::unchecked_key (nano::hash_or_account const & dependency) :
	unchecked_key{ dependency, 0 }
{
}

nano::unchecked_key::unchecked_key (nano::hash_or_account const & previous_a, nano::block_hash const & hash_a) :
	previous (previous_a.as_block_hash ()),
	hash (hash_a)
{
}

nano::unchecked_key::unchecked_key (nano::uint512_union const & union_a) :
	previous (union_a.uint256s[0].number ()),
	hash (union_a.uint256s[1].number ())
{
}

bool nano::unchecked_key::deserialize (nano::stream & stream_a)
{
	auto error (false);
	try
	{
		nano::read (stream_a, previous.bytes);
		nano::read (stream_a, hash.bytes);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

bool nano::unchecked_key::operator== (nano::unchecked_key const & other_a) const
{
	return previous == other_a.previous && hash == other_a.hash;
}

bool nano::unchecked_key::operator< (nano::unchecked_key const & other_a) const
{
	return previous != other_a.previous ? previous < other_a.previous : hash < other_a.hash;
}

nano::block_hash const & nano::unchecked_key::key () const
{
	return previous;
}

void nano::generate_cache::enable_all ()
{
	reps = true;
	cemented_count = true;
	unchecked_count = true;
	account_count = true;
}

nano::stat::detail nano::to_stat_detail (nano::process_result process_result)
{
	switch (process_result)
	{
		case process_result::progress:
			return nano::stat::detail::progress;
		case process_result::bad_signature:
			return nano::stat::detail::bad_signature;
		case process_result::old:
			return nano::stat::detail::old;
		case process_result::negative_spend:
			return nano::stat::detail::negative_spend;
		case process_result::fork:
			return nano::stat::detail::fork;
		case process_result::unreceivable:
			return nano::stat::detail::unreceivable;
		case process_result::gap_previous:
			return nano::stat::detail::gap_previous;
		case process_result::gap_source:
			return nano::stat::detail::gap_source;
		case process_result::gap_epoch_open_pending:
			return nano::stat::detail::gap_epoch_open_pending;
		case process_result::opened_burn_account:
			return nano::stat::detail::opened_burn_account;
		case process_result::balance_mismatch:
			return nano::stat::detail::balance_mismatch;
		case process_result::representative_mismatch:
			return nano::stat::detail::representative_mismatch;
		case process_result::block_position:
			return nano::stat::detail::block_position;
		case process_result::insufficient_work:
			return nano::stat::detail::insufficient_work;
	}
	debug_assert (false && "There should be always a defined nano::stat::detail that is not _last");
	return nano::stat::detail::_last;
}
