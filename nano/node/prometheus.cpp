#include <nano/node/prometheus.hpp>

#include <nano/node/active_transactions.hpp>
#include <nano/node/network.hpp>
#include <nano/node/unchecked_map.hpp>
#include <nano/secure/ledger.hpp>

#include <sstream>

std::string nano::to_prometheus_format (nano::stats & stats, nano::ledger & ledger, nano::network & network, nano::active_transactions & active, nano::unchecked_map & unchecked)
{
	std::ostringstream ss;

	// Block count
	ss << "# HELP kakitu_block_count Total number of blocks in ledger\n";
	ss << "# TYPE kakitu_block_count gauge\n";
	ss << "kakitu_block_count " << ledger.cache.block_count.load () << "\n\n";

	// Cemented count
	ss << "# HELP kakitu_cemented_count Number of cemented (confirmed) blocks\n";
	ss << "# TYPE kakitu_cemented_count gauge\n";
	ss << "kakitu_cemented_count " << ledger.cache.cemented_count.load () << "\n\n";

	// Active elections
	ss << "# HELP kakitu_active_elections Number of active elections\n";
	ss << "# TYPE kakitu_active_elections gauge\n";
	ss << "kakitu_active_elections " << active.size () << "\n\n";

	// Peer count
	ss << "# HELP kakitu_peer_count Number of connected peers\n";
	ss << "# TYPE kakitu_peer_count gauge\n";
	ss << "kakitu_peer_count " << network.size () << "\n\n";

	// Unchecked count
	ss << "# HELP kakitu_unchecked_count Number of unchecked blocks\n";
	ss << "# TYPE kakitu_unchecked_count gauge\n";
	ss << "kakitu_unchecked_count " << unchecked.count () << "\n\n";

	// Account count
	ss << "# HELP kakitu_account_count Total number of accounts in ledger\n";
	ss << "# TYPE kakitu_account_count gauge\n";
	ss << "kakitu_account_count " << ledger.cache.account_count.load () << "\n";

	return ss.str ();
}
