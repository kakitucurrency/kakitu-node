/**
 * Kakitu RPC Implementations for json_handler
 *
 * These implementations should be added to nano/node/json_handler.cpp
 * They follow the same pattern as other RPC handlers in that file.
 */

// Add to end of json_handler.cpp before the closing namespace

void nano::json_handler::supply_info ()
{
	if (!node.kakitu_mint_authority)
	{
		ec = nano::error_rpc::generic;
		response_l.put ("error", "Kakitu mint authority not initialized");
		response_errors ();
		return;
	}

	response_l.put ("total_supply", node.kakitu_mint_authority->get_total_supply ().to_string_dec ());
	response_l.put ("total_reserves", node.kakitu_mint_authority->get_total_reserves ().to_string_dec ());
	response_l.put ("backing_ratio", std::to_string (node.kakitu_mint_authority->get_backing_ratio ()));
	response_l.put ("is_balanced", node.kakitu_mint_authority->verify_reserves ());

	response_errors ();
}

void nano::json_handler::operation_history ()
{
	if (!node.kakitu_mint_authority)
	{
		ec = nano::error_rpc::generic;
		response_l.put ("error", "Kakitu mint authority not initialized");
		response_errors ();
		return;
	}

	uint64_t start_timestamp = 0;
	uint64_t end_timestamp = std::numeric_limits<uint64_t>::max ();

	boost::optional<std::string> start_text (request.get_optional<std::string> ("start_timestamp"));
	if (start_text.is_initialized ())
	{
		try
		{
			start_timestamp = std::stoull (start_text.get ());
		}
		catch (...)
		{
			ec = nano::error_rpc::invalid_timestamp;
			response_errors ();
			return;
		}
	}

	boost::optional<std::string> end_text (request.get_optional<std::string> ("end_timestamp"));
	if (end_text.is_initialized ())
	{
		try
		{
			end_timestamp = std::stoull (end_text.get ());
		}
		catch (...)
		{
			ec = nano::error_rpc::invalid_timestamp;
			response_errors ();
			return;
		}
	}

	auto operations = node.kakitu_mint_authority->get_operations_by_timerange (start_timestamp, end_timestamp);

	boost::property_tree::ptree operations_l;
	for (auto const & op : operations)
	{
		boost::property_tree::ptree operation;

		std::string type_str = op.type == nano::mint_operation_type::MINT ? "MINT" :
		                       op.type == nano::mint_operation_type::BURN ? "BURN" : "AUDIT";

		operation.put ("type", type_str);
		operation.put ("kshs_amount", op.kshs_amount.to_string_dec ());
		operation.put ("fiat_amount_kes", op.fiat_amount_kes.to_string_dec ());
		operation.put ("mpesa_transaction_id", op.mpesa_transaction_id);
		operation.put ("phone_number", op.phone_number);
		operation.put ("timestamp", std::to_string (op.timestamp));
		operation.put ("operation_hash", op.operation_hash.to_string ());

		if (op.type == nano::mint_operation_type::MINT)
		{
			operation.put ("destination", op.destination.to_account ());
		}
		else if (op.type == nano::mint_operation_type::BURN)
		{
			operation.put ("source", op.source.to_account ());
		}

		operations_l.push_back (std::make_pair ("", operation));
	}

	response_l.add_child ("operations", operations_l);
	response_errors ();
}

void nano::json_handler::verify_operation ()
{
	if (!node.kakitu_mint_authority)
	{
		ec = nano::error_rpc::generic;
		response_l.put ("error", "Kakitu mint authority not initialized");
		response_errors ();
		return;
	}

	std::string mpesa_tx_id = request.get<std::string> ("mpesa_transaction_id");

	auto operation_opt = node.kakitu_mint_authority->find_by_mpesa_id (mpesa_tx_id);

	if (!operation_opt.has_value ())
	{
		ec = nano::error_rpc::generic;
		response_l.put ("error", "Operation not found");
		response_errors ();
		return;
	}

	auto const & op = operation_opt.value ();

	bool valid = node.kakitu_mint_authority->verify_operation (op);

	response_l.put ("valid", valid);
	response_l.put ("type", op.type == nano::mint_operation_type::MINT ? "MINT" : "BURN");
	response_l.put ("kshs_amount", op.kshs_amount.to_string_dec ());
	response_l.put ("fiat_amount_kes", op.fiat_amount_kes.to_string_dec ());
	response_l.put ("mpesa_transaction_id", op.mpesa_transaction_id);
	response_l.put ("phone_number", op.phone_number);
	response_l.put ("timestamp", std::to_string (op.timestamp));
	response_l.put ("operation_hash", op.operation_hash.to_string ());
	response_l.put ("signature", op.signature.to_string ());

	if (op.type == nano::mint_operation_type::MINT)
	{
		response_l.put ("destination", op.destination.to_account ());
	}
	else
	{
		response_l.put ("source", op.source.to_account ());
	}

	response_errors ();
}

void nano::json_handler::reserve_status ()
{
	if (!node.kakitu_mpesa_bridge)
	{
		ec = nano::error_rpc::generic;
		response_l.put ("error", "Kakitu M-Pesa bridge not initialized");
		response_errors ();
		return;
	}

	auto status = node.kakitu_mpesa_bridge->get_reserve_status ();

	response_l.put ("mpesa_balance_kes", status.mpesa_balance_kes.to_string_dec ());
	response_l.put ("kshs_total_supply", status.kshs_total_supply.to_string_dec ());
	response_l.put ("pending_deposits", status.pending_deposits.to_string_dec ());
	response_l.put ("pending_withdrawals", status.pending_withdrawals.to_string_dec ());
	response_l.put ("is_balanced", status.is_balanced);
	response_l.put ("backing_ratio", std::to_string (status.backing_ratio));

	response_errors ();
}

void nano::json_handler::initiate_deposit ()
{
	if (!node.kakitu_mpesa_bridge)
	{
		ec = nano::error_rpc::generic;
		response_l.put ("error", "Kakitu M-Pesa bridge not initialized");
		response_errors ();
		return;
	}

	std::string phone_number = request.get<std::string> ("phone_number");
	nano::account account = account_impl ();
	if (ec)
	{
		response_errors ();
		return;
	}

	nano::uint128_t amount = amount_impl ();
	if (ec)
	{
		response_errors ();
		return;
	}

	std::string checkout_id = node.kakitu_mpesa_bridge->initiate_deposit (phone_number, account, amount);

	if (checkout_id.empty ())
	{
		ec = nano::error_rpc::generic;
		response_l.put ("error", "Failed to initiate deposit");
		response_errors ();
		return;
	}

	response_l.put ("checkout_request_id", checkout_id);
	response_l.put ("status", "pending");
	response_l.put ("message", "STK Push sent to phone. User should enter M-Pesa PIN.");

	response_errors ();
}

void nano::json_handler::audit_report ()
{
	if (!node.kakitu_mint_authority)
	{
		ec = nano::error_rpc::generic;
		response_l.put ("error", "Kakitu mint authority not initialized");
		response_errors ();
		return;
	}

	uint64_t start_timestamp = 0;
	uint64_t end_timestamp = std::numeric_limits<uint64_t>::max ();

	boost::optional<std::string> start_text (request.get_optional<std::string> ("start_timestamp"));
	if (start_text.is_initialized ())
	{
		try
		{
			start_timestamp = std::stoull (start_text.get ());
		}
		catch (...)
		{
			ec = nano::error_rpc::invalid_timestamp;
			response_errors ();
			return;
		}
	}

	boost::optional<std::string> end_text (request.get_optional<std::string> ("end_timestamp"));
	if (end_text.is_initialized ())
	{
		try
		{
			end_timestamp = std::stoull (end_text.get ());
		}
		catch (...)
		{
			ec = nano::error_rpc::invalid_timestamp;
			response_errors ();
			return;
		}
	}

	// Generate audit report
	Json::Value report = node.kakitu_mint_authority->export_audit_report (start_timestamp, end_timestamp);

	// Convert Json::Value to boost property tree
	response_l.put ("report_type", report.get ("report_type", "").asString ());
	response_l.put ("start_timestamp", report.get ("start_timestamp", 0).asUInt64 ());
	response_l.put ("end_timestamp", report.get ("end_timestamp", 0).asUInt64 ());
	response_l.put ("generated_at", report.get ("generated_at", 0).asUInt64 ());

	Json::Value summary = report["summary"];
	response_l.put ("total_supply_kshs", summary.get ("total_supply_kshs", "0").asString ());
	response_l.put ("total_reserves_kes", summary.get ("total_reserves_kes", "0").asString ());
	response_l.put ("backing_ratio", summary.get ("backing_ratio", 0.0).asDouble ());
	response_l.put ("is_balanced", summary.get ("is_balanced", false).asBool ());

	Json::Value period_stats = report["period_statistics"];
	response_l.put ("total_minted_kshs", period_stats.get ("total_minted_kshs", "0").asString ());
	response_l.put ("total_burned_kshs", period_stats.get ("total_burned_kshs", "0").asString ());
	response_l.put ("net_change_kshs", period_stats.get ("net_change_kshs", "0").asString ());
	response_l.put ("mint_operations", period_stats.get ("mint_operations", 0).asUInt ());
	response_l.put ("burn_operations", period_stats.get ("burn_operations", 0).asUInt ());

	response_errors ();
}
