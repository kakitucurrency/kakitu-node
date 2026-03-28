#pragma once

#include <string>

namespace nano
{
class stats;
class ledger;
class network;
class active_transactions;
class unchecked_map;

/**
 * Produces Prometheus text exposition format metrics from node state.
 * The output can be served at /metrics HTTP endpoint for scraping.
 */
std::string to_prometheus_format (nano::stats & stats, nano::ledger & ledger, nano::network & network, nano::active_transactions & active, nano::unchecked_map & unchecked);
}
