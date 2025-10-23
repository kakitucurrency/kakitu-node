#include <nano/rpc/rpc.hpp>
#include <nano/node/mint_authority.hpp>
#include <nano/node/mpesa_integration.hpp>

namespace nano {

// Add these RPC handlers to the existing RPC server

void rpc_handler::supply_info()
{
    // Get total supply and reserve information
    if (!mint_authority_) {
        error_response(response, "Mint authority not initialized");
        return;
    }

    response_l.put("total_supply", mint_authority_->get_total_supply().to_string_dec());
    response_l.put("total_reserves", mint_authority_->get_total_reserves().to_string_dec());
    response_l.put("backing_ratio", std::to_string(mint_authority_->get_backing_ratio()));
    response_l.put("is_balanced", mint_authority_->verify_reserves());

    response_errors();
}

void rpc_handler::operation_history()
{
    if (!mint_authority_) {
        error_response(response, "Mint authority not initialized");
        return;
    }

    uint64_t start_timestamp = 0;
    uint64_t end_timestamp = std::numeric_limits<uint64_t>::max();

    if (request.count("start_timestamp")) {
        start_timestamp = std::stoull(request.get<std::string>("start_timestamp"));
    }

    if (request.count("end_timestamp")) {
        end_timestamp = std::stoull(request.get<std::string>("end_timestamp"));
    }

    auto operations = mint_authority_->get_operations_by_timerange(start_timestamp, end_timestamp);

    boost::property_tree::ptree operations_l;
    for (auto const & op : operations) {
        boost::property_tree::ptree operation;

        std::string type_str = op.type == mint_operation_type::MINT ? "MINT" :
                               op.type == mint_operation_type::BURN ? "BURN" : "AUDIT";

        operation.put("type", type_str);
        operation.put("kshs_amount", op.kshs_amount.to_string_dec());
        operation.put("fiat_amount_kes", op.fiat_amount_kes.to_string_dec());
        operation.put("mpesa_transaction_id", op.mpesa_transaction_id);
        operation.put("phone_number", op.phone_number);
        operation.put("timestamp", std::to_string(op.timestamp));
        operation.put("operation_hash", op.operation_hash.to_string());

        if (op.type == mint_operation_type::MINT) {
            operation.put("destination", op.destination.to_account());
        } else if (op.type == mint_operation_type::BURN) {
            operation.put("source", op.source.to_account());
        }

        operations_l.push_back(std::make_pair("", operation));
    }

    response_l.add_child("operations", operations_l);
    response_errors();
}

void rpc_handler::verify_operation()
{
    if (!mint_authority_) {
        error_response(response, "Mint authority not initialized");
        return;
    }

    std::string mpesa_tx_id = request.get<std::string>("mpesa_transaction_id");

    auto operation_opt = mint_authority_->find_by_mpesa_id(mpesa_tx_id);

    if (!operation_opt.has_value()) {
        error_response(response, "Operation not found");
        return;
    }

    auto const & op = operation_opt.value();

    bool valid = mint_authority_->verify_operation(op);

    response_l.put("valid", valid);
    response_l.put("type", op.type == mint_operation_type::MINT ? "MINT" : "BURN");
    response_l.put("kshs_amount", op.kshs_amount.to_string_dec());
    response_l.put("fiat_amount_kes", op.fiat_amount_kes.to_string_dec());
    response_l.put("mpesa_transaction_id", op.mpesa_transaction_id);
    response_l.put("phone_number", op.phone_number);
    response_l.put("timestamp", std::to_string(op.timestamp));
    response_l.put("operation_hash", op.operation_hash.to_string());
    response_l.put("signature", op.signature.to_string());

    if (op.type == mint_operation_type::MINT) {
        response_l.put("destination", op.destination.to_account());
    } else {
        response_l.put("source", op.source.to_account());
    }

    response_errors();
}

void rpc_handler::reserve_status()
{
    if (!mpesa_bridge_) {
        error_response(response, "M-Pesa bridge not initialized");
        return;
    }

    auto status = mpesa_bridge_->get_reserve_status();

    response_l.put("mpesa_balance_kes", status.mpesa_balance_kes.to_string_dec());
    response_l.put("kshs_total_supply", status.kshs_total_supply.to_string_dec());
    response_l.put("pending_deposits", status.pending_deposits.to_string_dec());
    response_l.put("pending_withdrawals", status.pending_withdrawals.to_string_dec());
    response_l.put("is_balanced", status.is_balanced);
    response_l.put("backing_ratio", std::to_string(status.backing_ratio));

    response_errors();
}

void rpc_handler::initiate_deposit()
{
    if (!mpesa_bridge_) {
        error_response(response, "M-Pesa bridge not initialized");
        return;
    }

    std::string phone_number = request.get<std::string>("phone_number");
    std::string account_str = request.get<std::string>("account");
    std::string amount_str = request.get<std::string>("amount");

    nano::account account;
    if (account.decode_account(account_str)) {
        error_response(response, "Invalid account");
        return;
    }

    nano::uint128_t amount;
    if (amount.decode_dec(amount_str)) {
        error_response(response, "Invalid amount");
        return;
    }

    std::string checkout_id = mpesa_bridge_->initiate_deposit(phone_number, account, amount);

    if (checkout_id.empty()) {
        error_response(response, "Failed to initiate deposit");
        return;
    }

    response_l.put("checkout_request_id", checkout_id);
    response_l.put("status", "pending");
    response_l.put("message", "STK Push sent to phone. User should enter M-Pesa PIN.");

    response_errors();
}

void rpc_handler::audit_report()
{
    if (!mint_authority_) {
        error_response(response, "Mint authority not initialized");
        return;
    }

    uint64_t start_timestamp = 0;
    uint64_t end_timestamp = std::numeric_limits<uint64_t>::max();

    if (request.count("start_timestamp")) {
        start_timestamp = std::stoull(request.get<std::string>("start_timestamp"));
    }

    if (request.count("end_timestamp")) {
        end_timestamp = std::stoull(request.get<std::string>("end_timestamp"));
    }

    // Generate audit report
    Json::Value report = mint_authority_->export_audit_report(start_timestamp, end_timestamp);

    // Convert to boost property tree
    response_l.put("report_type", report.get("report_type", "").asString());
    response_l.put("start_timestamp", report.get("start_timestamp", 0).asUInt64());
    response_l.put("end_timestamp", report.get("end_timestamp", 0).asUInt64());
    response_l.put("generated_at", report.get("generated_at", 0).asUInt64());

    Json::Value summary = report["summary"];
    response_l.put("total_supply_kshs", summary.get("total_supply_kshs", "0").asString());
    response_l.put("total_reserves_kes", summary.get("total_reserves_kes", "0").asString());
    response_l.put("backing_ratio", summary.get("backing_ratio", 0.0).asDouble());
    response_l.put("is_balanced", summary.get("is_balanced", false).asBool());

    Json::Value period_stats = report["period_statistics"];
    response_l.put("total_minted_kshs", period_stats.get("total_minted_kshs", "0").asString());
    response_l.put("total_burned_kshs", period_stats.get("total_burned_kshs", "0").asString());
    response_l.put("net_change_kshs", period_stats.get("net_change_kshs", "0").asString());
    response_l.put("mint_operations", period_stats.get("mint_operations", 0).asUInt());
    response_l.put("burn_operations", period_stats.get("burn_operations", 0).asUInt());

    response_errors();
}

// Example of how to add these handlers to the RPC server
// This would go in the main RPC handler registration

/*
void rpc_connection::register_kakitu_handlers()
{
    handlers["supply_info"] = [this]() {
        supply_info();
    };

    handlers["operation_history"] = [this]() {
        operation_history();
    };

    handlers["verify_operation"] = [this]() {
        verify_operation();
    };

    handlers["reserve_status"] = [this]() {
        reserve_status();
    };

    handlers["initiate_deposit"] = [this]() {
        initiate_deposit();
    };

    handlers["audit_report"] = [this]() {
        audit_report();
    };
}
*/

} // namespace nano
