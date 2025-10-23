#include <nano/node/mint_authority.hpp>
#include <nano/lib/blocks.hpp>
#include <nano/lib/numbers.hpp>
#include <nano/crypto/blake2/blake2.h>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace nano {

// ============================================
// mint_operation Implementation
// ============================================

nano::block_hash mint_operation::calculate_hash() const
{
    // Hash all fields to create tamper-evident proof
    nano::blake2b_state hash;
    blake2b_init(&hash, sizeof(nano::block_hash));

    // Hash type
    auto type_val = static_cast<uint8_t>(type);
    blake2b_update(&hash, &type_val, sizeof(type_val));

    // Hash amounts
    blake2b_update(&hash, kshs_amount.bytes.data(), kshs_amount.bytes.size());
    blake2b_update(&hash, fiat_amount_kes.bytes.data(), fiat_amount_kes.bytes.size());

    // Hash accounts
    blake2b_update(&hash, destination.bytes.data(), destination.bytes.size());
    blake2b_update(&hash, source.bytes.data(), source.bytes.size());

    // Hash M-Pesa data
    blake2b_update(&hash, mpesa_transaction_id.data(), mpesa_transaction_id.size());
    blake2b_update(&hash, phone_number.data(), phone_number.size());

    // Hash timestamps
    blake2b_update(&hash, &timestamp, sizeof(timestamp));
    blake2b_update(&hash, &mpesa_timestamp, sizeof(mpesa_timestamp));

    // Hash merkle root
    blake2b_update(&hash, merkle_root.bytes.data(), merkle_root.bytes.size());

    // Hash log index
    blake2b_update(&hash, &log_index, sizeof(log_index));

    nano::block_hash result;
    blake2b_final(&hash, result.bytes.data(), result.bytes.size());

    return result;
}

bool mint_operation::verify_signature(nano::public_key const & mint_authority_key) const
{
    // Verify the mint authority signed this operation
    return nano::validate_message(mint_authority_key, operation_hash, signature);
}

Json::Value mint_operation::to_json() const
{
    Json::Value json;

    // Type
    std::string type_str;
    switch (type) {
        case mint_operation_type::MINT: type_str = "MINT"; break;
        case mint_operation_type::BURN: type_str = "BURN"; break;
        case mint_operation_type::AUDIT: type_str = "AUDIT"; break;
    }
    json["type"] = type_str;

    // Amounts
    json["kshs_amount"] = kshs_amount.to_string_dec();
    json["fiat_amount_kes"] = fiat_amount_kes.to_string_dec();

    // Accounts
    json["destination"] = destination.to_account();
    json["source"] = source.to_account();
    json["burn_address"] = burn_address.to_account();

    // M-Pesa data
    json["mpesa_transaction_id"] = mpesa_transaction_id;
    json["phone_number"] = phone_number;
    json["mpesa_first_name"] = mpesa_first_name;
    json["mpesa_last_name"] = mpesa_last_name;

    // Timestamps
    json["timestamp"] = static_cast<Json::Value::UInt64>(timestamp);
    json["mpesa_timestamp"] = static_cast<Json::Value::UInt64>(mpesa_timestamp);

    // Cryptographic proofs
    json["operation_hash"] = operation_hash.to_string();
    json["block_hash"] = block_hash.to_string();
    json["signature"] = signature.to_string();
    json["merkle_root"] = merkle_root.to_string();
    json["log_index"] = static_cast<Json::Value::UInt64>(log_index);

    // Metadata
    json["notes"] = notes;

    return json;
}

mint_operation mint_operation::from_json(Json::Value const & json)
{
    mint_operation op;

    // Type
    std::string type_str = json["type"].asString();
    if (type_str == "MINT") op.type = mint_operation_type::MINT;
    else if (type_str == "BURN") op.type = mint_operation_type::BURN;
    else if (type_str == "AUDIT") op.type = mint_operation_type::AUDIT;

    // Amounts
    op.kshs_amount.decode_dec(json["kshs_amount"].asString());
    op.fiat_amount_kes.decode_dec(json["fiat_amount_kes"].asString());

    // Accounts
    op.destination.decode_account(json["destination"].asString());
    op.source.decode_account(json["source"].asString());
    op.burn_address.decode_account(json["burn_address"].asString());

    // M-Pesa data
    op.mpesa_transaction_id = json["mpesa_transaction_id"].asString();
    op.phone_number = json["phone_number"].asString();
    op.mpesa_first_name = json["mpesa_first_name"].asString();
    op.mpesa_last_name = json["mpesa_last_name"].asString();

    // Timestamps
    op.timestamp = json["timestamp"].asUInt64();
    op.mpesa_timestamp = json["mpesa_timestamp"].asUInt64();

    // Cryptographic proofs
    op.operation_hash.decode_hex(json["operation_hash"].asString());
    op.block_hash.decode_hex(json["block_hash"].asString());
    op.signature.decode_hex(json["signature"].asString());
    op.merkle_root.decode_hex(json["merkle_root"].asString());
    op.log_index = json["log_index"].asUInt64();

    // Metadata
    op.notes = json.get("notes", "").asString();

    return op;
}

// ============================================
// mint_authority Implementation
// ============================================

mint_authority::mint_authority(nano::keypair const & authority_key)
    : authority_key_(authority_key)
    , last_reset_timestamp_(std::chrono::system_clock::now().time_since_epoch().count())
{
}

std::shared_ptr<mint_authority> mint_authority::load_from_disk(
    std::string const & data_path,
    nano::keypair const & authority_key)
{
    auto auth = std::make_shared<mint_authority>(authority_key);
    auth->load_from_disk(data_path);
    return auth;
}

mint_operation mint_authority::create_mint(
    nano::account const & destination,
    nano::uint128_t kshs_amount,
    std::string const & mpesa_tx_id,
    nano::uint128_t fiat_amount_kes,
    std::string const & phone_number,
    std::string const & mpesa_first_name,
    std::string const & mpesa_last_name)
{
    check_and_reset_daily_limits();

    // Check daily limit
    if (!is_within_mint_limit(kshs_amount)) {
        throw std::runtime_error("Mint amount exceeds daily limit");
    }

    mint_operation op;
    op.type = mint_operation_type::MINT;
    op.kshs_amount = kshs_amount;
    op.fiat_amount_kes = fiat_amount_kes;
    op.destination = destination;
    op.mpesa_transaction_id = mpesa_tx_id;
    op.phone_number = phone_number;
    op.mpesa_first_name = mpesa_first_name;
    op.mpesa_last_name = mpesa_last_name;
    op.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000; // milliseconds
    op.mpesa_timestamp = op.timestamp;
    op.log_index = operation_log_.size();

    // Calculate hash
    op.operation_hash = op.calculate_hash();

    // Sign the operation
    op.signature = sign_operation(op);

    // Update merkle tree
    update_merkle_tree(op.operation_hash);
    op.merkle_root = calculate_merkle_root();

    // Update supply tracking
    total_kshs_supply_ += kshs_amount;
    total_kes_reserves_ += fiat_amount_kes;
    daily_minted_today_ += kshs_amount;

    // Log the operation
    operation_log_.push_back(op);

    return op;
}

mint_operation mint_authority::create_burn(
    nano::account const & source,
    nano::uint128_t kshs_amount,
    std::string const & mpesa_tx_id,
    nano::uint128_t fiat_amount_kes,
    std::string const & phone_number)
{
    check_and_reset_daily_limits();

    // Check daily limit
    if (!is_within_burn_limit(kshs_amount)) {
        throw std::runtime_error("Burn amount exceeds daily limit");
    }

    mint_operation op;
    op.type = mint_operation_type::BURN;
    op.kshs_amount = kshs_amount;
    op.fiat_amount_kes = fiat_amount_kes;
    op.source = source;
    op.mpesa_transaction_id = mpesa_tx_id;
    op.phone_number = phone_number;
    op.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
    op.mpesa_timestamp = op.timestamp;
    op.log_index = operation_log_.size();

    // Calculate hash
    op.operation_hash = op.calculate_hash();

    // Sign the operation
    op.signature = sign_operation(op);

    // Update merkle tree
    update_merkle_tree(op.operation_hash);
    op.merkle_root = calculate_merkle_root();

    // Update supply tracking
    total_kshs_supply_ -= kshs_amount;
    total_kes_reserves_ -= fiat_amount_kes;
    daily_burned_today_ += kshs_amount;

    // Log the operation
    operation_log_.push_back(op);

    return op;
}

mint_operation mint_authority::create_audit(
    nano::uint128_t mpesa_balance,
    nano::uint128_t kshs_supply,
    std::string const & notes)
{
    mint_operation op;
    op.type = mint_operation_type::AUDIT;
    op.fiat_amount_kes = mpesa_balance;
    op.kshs_amount = kshs_supply;
    op.notes = notes;
    op.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
    op.log_index = operation_log_.size();

    // Calculate hash
    op.operation_hash = op.calculate_hash();

    // Sign the operation
    op.signature = sign_operation(op);

    // Update merkle tree
    update_merkle_tree(op.operation_hash);
    op.merkle_root = calculate_merkle_root();

    // Log the operation
    operation_log_.push_back(op);

    return op;
}

double mint_authority::get_backing_ratio() const
{
    if (total_kshs_supply_.is_zero()) {
        return 1.0;
    }

    auto supply_double = static_cast<double>(total_kshs_supply_.convert_to<uint64_t>());
    auto reserves_double = static_cast<double>(total_kes_reserves_.convert_to<uint64_t>());

    return reserves_double / supply_double;
}

std::vector<mint_operation> mint_authority::get_operations_by_type(
    mint_operation_type type) const
{
    std::vector<mint_operation> result;
    for (auto const & op : operation_log_) {
        if (op.type == type) {
            result.push_back(op);
        }
    }
    return result;
}

std::vector<mint_operation> mint_authority::get_operations_by_timerange(
    uint64_t start_timestamp,
    uint64_t end_timestamp) const
{
    std::vector<mint_operation> result;
    for (auto const & op : operation_log_) {
        if (op.timestamp >= start_timestamp && op.timestamp <= end_timestamp) {
            result.push_back(op);
        }
    }
    return result;
}

std::optional<mint_operation> mint_authority::find_by_mpesa_id(
    std::string const & mpesa_tx_id) const
{
    for (auto const & op : operation_log_) {
        if (op.mpesa_transaction_id == mpesa_tx_id) {
            return op;
        }
    }
    return std::nullopt;
}

std::optional<mint_operation> mint_authority::find_by_block_hash(
    nano::block_hash const & hash) const
{
    for (auto const & op : operation_log_) {
        if (op.block_hash == hash) {
            return op;
        }
    }
    return std::nullopt;
}

bool mint_authority::save_to_disk(std::string const & data_path)
{
    try {
        Json::Value root;
        root["version"] = 1;
        root["total_kshs_supply"] = total_kshs_supply_.to_string_dec();
        root["total_kes_reserves"] = total_kes_reserves_.to_string_dec();
        root["mint_authority_public_key"] = authority_key_.pub.to_account();

        Json::Value operations(Json::arrayValue);
        for (auto const & op : operation_log_) {
            operations.append(op.to_json());
        }
        root["operations"] = operations;

        std::ofstream file(data_path);
        file << root;
        file.close();

        return true;
    } catch (...) {
        return false;
    }
}

bool mint_authority::load_from_disk(std::string const & data_path)
{
    try {
        std::ifstream file(data_path);
        Json::Value root;
        file >> root;
        file.close();

        total_kshs_supply_.decode_dec(root["total_kshs_supply"].asString());
        total_kes_reserves_.decode_dec(root["total_kes_reserves"].asString());

        operation_log_.clear();
        Json::Value operations = root["operations"];
        for (auto const & op_json : operations) {
            operation_log_.push_back(mint_operation::from_json(op_json));
        }

        // Rebuild merkle tree
        merkle_tree_.clear();
        for (auto const & op : operation_log_) {
            update_merkle_tree(op.operation_hash);
        }

        return true;
    } catch (...) {
        return false;
    }
}

Json::Value mint_authority::export_audit_report(
    uint64_t start_timestamp,
    uint64_t end_timestamp) const
{
    Json::Value report;
    report["report_type"] = "KAKITU_MINT_AUDIT";
    report["start_timestamp"] = static_cast<Json::Value::UInt64>(start_timestamp);
    report["end_timestamp"] = static_cast<Json::Value::UInt64>(end_timestamp);
    report["generated_at"] = static_cast<Json::Value::UInt64>(
        std::chrono::system_clock::now().time_since_epoch().count() / 1000000
    );

    // Summary statistics
    Json::Value summary;
    summary["total_supply_kshs"] = total_kshs_supply_.to_string_dec();
    summary["total_reserves_kes"] = total_kes_reserves_.to_string_dec();
    summary["backing_ratio"] = get_backing_ratio();
    summary["is_balanced"] = verify_reserves();
    report["summary"] = summary;

    // Get operations in time range
    auto ops = get_operations_by_timerange(start_timestamp, end_timestamp);

    nano::uint128_t total_minted{0};
    nano::uint128_t total_burned{0};
    size_t mint_count = 0;
    size_t burn_count = 0;

    Json::Value operations(Json::arrayValue);
    for (auto const & op : ops) {
        operations.append(op.to_json());

        if (op.type == mint_operation_type::MINT) {
            total_minted += op.kshs_amount;
            mint_count++;
        } else if (op.type == mint_operation_type::BURN) {
            total_burned += op.kshs_amount;
            burn_count++;
        }
    }
    report["operations"] = operations;

    // Period statistics
    Json::Value period_stats;
    period_stats["total_minted_kshs"] = total_minted.to_string_dec();
    period_stats["total_burned_kshs"] = total_burned.to_string_dec();
    period_stats["net_change_kshs"] = (total_minted - total_burned).to_string_dec();
    period_stats["mint_operations"] = static_cast<Json::Value::UInt>(mint_count);
    period_stats["burn_operations"] = static_cast<Json::Value::UInt>(burn_count);
    report["period_statistics"] = period_stats;

    return report;
}

std::string mint_authority::export_to_csv(
    uint64_t start_timestamp,
    uint64_t end_timestamp) const
{
    std::ostringstream csv;

    // Header
    csv << "Type,Timestamp,KSHS Amount,KES Amount,M-Pesa TX ID,Phone,Account,Operation Hash\n";

    // Get operations
    auto ops = get_operations_by_timerange(start_timestamp, end_timestamp);

    for (auto const & op : ops) {
        std::string type_str = op.type == mint_operation_type::MINT ? "MINT" :
                               op.type == mint_operation_type::BURN ? "BURN" : "AUDIT";

        std::string account = op.type == mint_operation_type::MINT ?
                             op.destination.to_account() : op.source.to_account();

        csv << type_str << ","
            << op.timestamp << ","
            << op.kshs_amount.to_string_dec() << ","
            << op.fiat_amount_kes.to_string_dec() << ","
            << op.mpesa_transaction_id << ","
            << op.phone_number << ","
            << account << ","
            << op.operation_hash.to_string() << "\n";
    }

    return csv.str();
}

bool mint_authority::verify_log_integrity() const
{
    // Rebuild merkle tree and verify root
    std::vector<nano::block_hash> temp_tree;

    for (auto const & op : operation_log_) {
        // Verify operation hash
        if (op.operation_hash != op.calculate_hash()) {
            return false;
        }

        // Verify signature
        if (!op.verify_signature(authority_key_.pub)) {
            return false;
        }

        // Add to temp merkle tree
        temp_tree.push_back(op.operation_hash);
    }

    // Verify merkle tree matches
    if (temp_tree.size() != merkle_tree_.size()) {
        return false;
    }

    for (size_t i = 0; i < temp_tree.size(); ++i) {
        if (temp_tree[i] != merkle_tree_[i]) {
            return false;
        }
    }

    return true;
}

bool mint_authority::verify_operation(mint_operation const & op) const
{
    // Verify hash
    if (op.operation_hash != op.calculate_hash()) {
        return false;
    }

    // Verify signature
    if (!op.verify_signature(authority_key_.pub)) {
        return false;
    }

    return true;
}

bool mint_authority::is_within_mint_limit(nano::uint128_t amount) const
{
    if (daily_mint_limit_.is_zero()) {
        return true; // No limit
    }

    return (daily_minted_today_ + amount) <= daily_mint_limit_;
}

bool mint_authority::is_within_burn_limit(nano::uint128_t amount) const
{
    if (daily_burn_limit_.is_zero()) {
        return true; // No limit
    }

    return (daily_burned_today_ + amount) <= daily_burn_limit_;
}

void mint_authority::reset_daily_limits()
{
    daily_minted_today_ = 0;
    daily_burned_today_ = 0;
    last_reset_timestamp_ = std::chrono::system_clock::now().time_since_epoch().count();
}

nano::signature mint_authority::sign_operation(mint_operation const & op) const
{
    return nano::sign_message(authority_key_.prv, authority_key_.pub, op.operation_hash);
}

nano::block_hash mint_authority::calculate_merkle_root()
{
    if (merkle_tree_.empty()) {
        return nano::block_hash{0};
    }

    if (merkle_tree_.size() == 1) {
        return merkle_tree_[0];
    }

    // Simple merkle root: hash all operation hashes together
    nano::blake2b_state hash;
    blake2b_init(&hash, sizeof(nano::block_hash));

    for (auto const & op_hash : merkle_tree_) {
        blake2b_update(&hash, op_hash.bytes.data(), op_hash.bytes.size());
    }

    nano::block_hash result;
    blake2b_final(&hash, result.bytes.data(), result.bytes.size());

    return result;
}

void mint_authority::update_merkle_tree(nano::block_hash const & operation_hash)
{
    merkle_tree_.push_back(operation_hash);
}

void mint_authority::check_and_reset_daily_limits()
{
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    auto elapsed = now - last_reset_timestamp_;

    // Reset if more than 24 hours (86400000000000 nanoseconds)
    if (elapsed > 86400000000000) {
        reset_daily_limits();
    }
}

// ============================================
// mint_operation_validator Implementation
// ============================================

mint_operation_validator::mint_operation_validator(nano::public_key const & mint_authority_key)
    : mint_authority_key_(mint_authority_key)
{
}

bool mint_operation_validator::validate(mint_operation const & op) const
{
    return validate_hash(op) && validate_signature(op);
}

bool mint_operation_validator::validate_signature(mint_operation const & op) const
{
    return op.verify_signature(mint_authority_key_);
}

bool mint_operation_validator::validate_hash(mint_operation const & op) const
{
    return op.operation_hash == op.calculate_hash();
}

} // namespace nano
