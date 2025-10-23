#include <nano/node/mpesa_integration.hpp>
#include <nano/crypto/blake2/blake2.h>

#include <curl/curl.h>
#include <json/json.h>

#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace nano {

// ============================================
// mpesa_transaction Implementation
// ============================================

nano::block_hash mpesa_transaction::calculate_hash() const
{
    nano::blake2b_state hash;
    blake2b_init(&hash, sizeof(nano::block_hash));

    blake2b_update(&hash, transaction_id.data(), transaction_id.size());
    blake2b_update(&hash, phone_number.data(), phone_number.size());
    blake2b_update(&hash, amount_kes.bytes.data(), amount_kes.bytes.size());

    auto type_val = static_cast<uint8_t>(type);
    blake2b_update(&hash, &type_val, sizeof(type_val));
    blake2b_update(&hash, &timestamp, sizeof(timestamp));

    nano::block_hash result;
    blake2b_final(&hash, result.bytes.data(), result.bytes.size());

    return result;
}

// ============================================
// HTTP Helper Functions
// ============================================

namespace {

// Callback for curl to write response data
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Base64 encode
std::string base64_encode(std::string const & input)
{
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input.c_str(), input.length());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    return result;
}

} // anonymous namespace

// ============================================
// mpesa_api_client Implementation
// ============================================

mpesa_api_client::mpesa_api_client(
    std::string const & consumer_key,
    std::string const & consumer_secret,
    std::string const & shortcode,
    std::string const & passkey,
    bool is_sandbox)
    : consumer_key_(consumer_key)
    , consumer_secret_(consumer_secret)
    , shortcode_(shortcode)
    , passkey_(passkey)
    , token_expiry_(0)
{
    base_url_ = is_sandbox ?
        "https://sandbox.safaricom.co.ke" :
        "https://api.safaricom.co.ke";

    curl_handle_ = curl_easy_init();
}

mpesa_api_client::~mpesa_api_client()
{
    if (curl_handle_) {
        curl_easy_cleanup(curl_handle_);
    }
}

bool mpesa_api_client::authenticate()
{
    auto now = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000; // seconds

    // Token valid for 1 hour, refresh if expired
    if (now < token_expiry_ && !access_token_.empty()) {
        return true;
    }

    std::string auth_url = base_url_ + "/oauth/v1/generate?grant_type=client_credentials";
    std::string credentials = consumer_key_ + ":" + consumer_secret_;
    std::string auth_header = "Authorization: Basic " + base64_encode(credentials);

    curl_easy_setopt(curl_handle_, CURLOPT_URL, auth_url.c_str());

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, auth_header.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, headers);

    std::string response_string;
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response_string);

    CURLcode res = curl_easy_perform(curl_handle_);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        return false;
    }

    // Parse response
    Json::Value response;
    Json::CharReaderBuilder builder;
    std::istringstream s(response_string);
    std::string errs;

    if (!Json::parseFromStream(builder, s, &response, &errs)) {
        return false;
    }

    if (!response.isMember("access_token")) {
        return false;
    }

    access_token_ = response["access_token"].asString();
    int expires_in = response.get("expires_in", 3600).asInt();
    token_expiry_ = now + expires_in;

    return true;
}

Json::Value mpesa_api_client::make_request(
    std::string const & endpoint,
    std::string const & method,
    Json::Value const & payload)
{
    if (!authenticate()) {
        Json::Value error;
        error["error"] = "Authentication failed";
        return error;
    }

    std::string url = base_url_ + endpoint;

    curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());

    // Set headers
    struct curl_slist* headers = NULL;
    std::string auth_header = "Authorization: Bearer " + access_token_;
    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, headers);

    // Set method and payload
    if (method == "POST") {
        Json::StreamWriterBuilder writer;
        std::string payload_str = Json::writeString(writer, payload);

        curl_easy_setopt(curl_handle_, CURLOPT_POST, 1L);
        curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, payload_str.c_str());
    }

    // Response callback
    std::string response_string;
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response_string);

    CURLcode res = curl_easy_perform(curl_handle_);
    curl_slist_free_all(headers);

    // Parse response
    Json::Value response;
    if (res == CURLE_OK && !response_string.empty()) {
        Json::CharReaderBuilder builder;
        std::istringstream s(response_string);
        std::string errs;
        Json::parseFromStream(builder, s, &response, &errs);
    } else {
        response["error"] = "HTTP request failed";
        response["curl_error"] = curl_easy_strerror(res);
    }

    return response;
}

std::string mpesa_api_client::generate_password() const
{
    std::string timestamp = generate_timestamp();
    std::string data = shortcode_ + passkey_ + timestamp;
    return base64_encode(data);
}

std::string mpesa_api_client::generate_timestamp() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now = *std::localtime(&time_t_now);

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y%m%d%H%M%S");
    return oss.str();
}

bool mpesa_api_client::register_c2b_urls(
    std::string const & validation_url,
    std::string const & confirmation_url)
{
    Json::Value payload;
    payload["ShortCode"] = shortcode_;
    payload["ResponseType"] = "Completed";
    payload["ConfirmationURL"] = confirmation_url;
    payload["ValidationURL"] = validation_url;

    Json::Value response = make_request("/mpesa/c2b/v1/registerurl", "POST", payload);

    return response.get("ResponseCode", "1").asString() == "0" ||
           response.get("ResponseDescription", "").asString().find("Success") != std::string::npos;
}

mpesa_transaction mpesa_api_client::process_c2b_callback(Json::Value const & callback_data)
{
    mpesa_transaction tx;
    tx.type = mpesa_transaction_type::C2B_DEPOSIT;
    tx.transaction_id = callback_data.get("TransID", "").asString();
    tx.phone_number = callback_data.get("MSISDN", "").asString();
    tx.first_name = callback_data.get("FirstName", "").asString();
    tx.last_name = callback_data.get("LastName", "").asString();
    tx.business_short_code = callback_data.get("BusinessShortCode", "").asString();
    tx.bill_ref_number = callback_data.get("BillRefNumber", "").asString();

    std::string amount_str = callback_data.get("TransAmount", "0").asString();
    tx.amount_kes.decode_dec(amount_str);

    std::string time_str = callback_data.get("TransTime", "").asString();
    // Parse time string (format: YYYYMMDDHHmmss)
    // For simplicity, use current time
    tx.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
    tx.mpesa_timestamp = tx.timestamp;

    tx.status = mpesa_status::COMPLETED;
    tx.proof_hash = tx.calculate_hash();

    return tx;
}

std::string mpesa_api_client::initiate_stk_push(
    std::string const & phone_number,
    nano::uint128_t amount_kes,
    std::string const & account_reference,
    std::string const & description)
{
    std::string timestamp = generate_timestamp();
    std::string password = generate_password();

    Json::Value payload;
    payload["BusinessShortCode"] = shortcode_;
    payload["Password"] = password;
    payload["Timestamp"] = timestamp;
    payload["TransactionType"] = "CustomerPayBillOnline";
    payload["Amount"] = amount_kes.to_string_dec();
    payload["PartyA"] = phone_number;
    payload["PartyB"] = shortcode_;
    payload["PhoneNumber"] = phone_number;
    payload["CallBackURL"] = "https://yourdomain.com/mpesa/stk-callback"; // TODO: Make configurable
    payload["AccountReference"] = account_reference;
    payload["TransactionDesc"] = description;

    Json::Value response = make_request("/mpesa/stkpush/v1/processrequest", "POST", payload);

    return response.get("CheckoutRequestID", "").asString();
}

mpesa_status mpesa_api_client::query_stk_status(std::string const & checkout_request_id)
{
    std::string timestamp = generate_timestamp();
    std::string password = generate_password();

    Json::Value payload;
    payload["BusinessShortCode"] = shortcode_;
    payload["Password"] = password;
    payload["Timestamp"] = timestamp;
    payload["CheckoutRequestID"] = checkout_request_id;

    Json::Value response = make_request("/mpesa/stkpushquery/v1/query", "POST", payload);

    std::string result_code = response.get("ResultCode", "1").asString();
    if (result_code == "0") {
        return mpesa_status::COMPLETED;
    } else if (result_code == "1032") {
        return mpesa_status::TIMEOUT;
    } else {
        return mpesa_status::FAILED;
    }
}

std::string mpesa_api_client::send_b2c_payment(
    std::string const & phone_number,
    nano::uint128_t amount_kes,
    std::string const & remarks,
    std::string const & occasion)
{
    Json::Value payload;
    payload["InitiatorName"] = "testapi"; // TODO: Make configurable
    payload["SecurityCredential"] = "ENCRYPTED_CREDENTIAL"; // TODO: Implement encryption
    payload["CommandID"] = "BusinessPayment";
    payload["Amount"] = amount_kes.to_string_dec();
    payload["PartyA"] = shortcode_;
    payload["PartyB"] = phone_number;
    payload["Remarks"] = remarks;
    payload["QueueTimeOutURL"] = "https://yourdomain.com/mpesa/b2c-timeout";
    payload["ResultURL"] = "https://yourdomain.com/mpesa/b2c-result";
    payload["Occasion"] = occasion;

    Json::Value response = make_request("/mpesa/b2c/v1/paymentrequest", "POST", payload);

    return response.get("ConversationID", "").asString();
}

mpesa_status mpesa_api_client::query_b2c_status(std::string const & transaction_id)
{
    // Implementation would query transaction status API
    // For now, return pending
    return mpesa_status::PENDING;
}

std::vector<std::string> mpesa_api_client::send_bulk_b2c(
    std::vector<std::pair<std::string, nano::uint128_t>> const & payments)
{
    std::vector<std::string> transaction_ids;

    for (auto const & payment : payments) {
        std::string tx_id = send_b2c_payment(
            payment.first,
            payment.second,
            "KSHS Withdrawal",
            "BULK_REDEMPTION"
        );
        transaction_ids.push_back(tx_id);
    }

    return transaction_ids;
}

nano::uint128_t mpesa_api_client::get_account_balance()
{
    Json::Value payload;
    payload["Initiator"] = "testapi";
    payload["SecurityCredential"] = "ENCRYPTED_CREDENTIAL";
    payload["CommandID"] = "AccountBalance";
    payload["PartyA"] = shortcode_;
    payload["IdentifierType"] = "4";
    payload["Remarks"] = "Balance query";
    payload["QueueTimeOutURL"] = "https://yourdomain.com/mpesa/timeout";
    payload["ResultURL"] = "https://yourdomain.com/mpesa/result";

    Json::Value response = make_request("/mpesa/accountbalance/v1/query", "POST", payload);

    // Balance will come in callback, for now return 0
    return nano::uint128_t{0};
}

mpesa_transaction mpesa_api_client::query_transaction(std::string const & transaction_id)
{
    mpesa_transaction tx;
    tx.transaction_id = transaction_id;
    tx.status = mpesa_status::PENDING;
    // TODO: Implement actual query
    return tx;
}

std::vector<mpesa_transaction> mpesa_api_client::get_transaction_history(
    uint64_t start_timestamp,
    uint64_t end_timestamp)
{
    // TODO: Implement transaction history query
    return std::vector<mpesa_transaction>();
}

bool mpesa_api_client::validate_phone_number(std::string const & phone)
{
    // Must be 12 digits starting with 254
    if (phone.length() != 12) return false;
    if (phone.substr(0, 3) != "254") return false;

    for (char c : phone) {
        if (!std::isdigit(c)) return false;
    }

    return true;
}

std::string mpesa_api_client::format_phone_number(std::string const & phone)
{
    // Remove any spaces, dashes, or plus signs
    std::string formatted;
    for (char c : phone) {
        if (std::isdigit(c)) {
            formatted += c;
        }
    }

    // Handle different formats
    if (formatted.length() == 10 && formatted[0] == '0') {
        // 0712345678 -> 254712345678
        return "254" + formatted.substr(1);
    } else if (formatted.length() == 9) {
        // 712345678 -> 254712345678
        return "254" + formatted;
    } else if (formatted.length() == 12 && formatted.substr(0, 3) == "254") {
        // Already correct format
        return formatted;
    } else if (formatted.length() == 13 && formatted[0] == '+') {
        // +254712345678 -> 254712345678
        return formatted.substr(1);
    }

    return formatted;
}

// ============================================
// kakitu_mpesa_bridge Implementation
// ============================================

kakitu_mpesa_bridge::kakitu_mpesa_bridge(
    std::shared_ptr<mpesa_api_client> mpesa_client,
    std::shared_ptr<nano::mint_authority> mint_authority)
    : mpesa_client_(mpesa_client)
    , mint_authority_(mint_authority)
{
}

bool kakitu_mpesa_bridge::process_deposit(
    mpesa_transaction const & mpesa_tx,
    nano::account const & destination_account)
{
    // Verify transaction not already processed
    auto existing = mint_authority_->find_by_mpesa_id(mpesa_tx.transaction_id);
    if (existing.has_value()) {
        return false; // Duplicate transaction
    }

    // Verify M-Pesa transaction is completed
    if (mpesa_tx.status != mpesa_status::COMPLETED) {
        return false;
    }

    // Calculate KSHS amount (1:1 with KES)
    nano::uint128_t kshs_amount = mpesa_tx.amount_kes;

    try {
        // Create mint operation
        auto mint_op = mint_authority_->create_mint(
            destination_account,
            kshs_amount,
            mpesa_tx.transaction_id,
            mpesa_tx.amount_kes,
            mpesa_tx.phone_number,
            mpesa_tx.first_name,
            mpesa_tx.last_name
        );

        // TODO: Create and broadcast block to Kakitu network
        // This would involve creating a special "mint" block type
        // and sending it to the user's account

        // Save transaction mapping
        mpesa_to_kakitu_map_[mpesa_tx.transaction_id] = destination_account;
        save_transaction_mapping();

        // Call deposit callback if set
        if (on_deposit_callback_) {
            on_deposit_callback_(mpesa_tx);
        }

        return true;
    } catch (std::exception const & e) {
        // Log error
        return false;
    }
}

std::string kakitu_mpesa_bridge::initiate_deposit(
    std::string const & phone_number,
    nano::account const & kakitu_account,
    nano::uint128_t amount_kes)
{
    // Format phone number
    std::string formatted_phone = mpesa_api_client::format_phone_number(phone_number);

    if (!mpesa_api_client::validate_phone_number(formatted_phone)) {
        return "";
    }

    // Initiate STK Push
    std::string checkout_id = mpesa_client_->initiate_stk_push(
        formatted_phone,
        amount_kes,
        kakitu_account.to_account(),
        "Buy KSHS"
    );

    if (!checkout_id.empty()) {
        // Track pending transaction
        mpesa_transaction pending_tx;
        pending_tx.transaction_id = checkout_id;
        pending_tx.phone_number = formatted_phone;
        pending_tx.amount_kes = amount_kes;
        pending_tx.kakitu_account = kakitu_account;
        pending_tx.status = mpesa_status::PENDING;
        pending_tx.type = mpesa_transaction_type::C2B_DEPOSIT;

        pending_transactions_[checkout_id] = pending_tx;
        total_pending_deposits_ += amount_kes;
    }

    return checkout_id;
}

bool kakitu_mpesa_bridge::process_withdrawal(
    nano::block_hash const & burn_block_hash,
    std::string const & phone_number,
    nano::uint128_t kshs_amount)
{
    // Calculate KES amount (1:1)
    nano::uint128_t kes_amount = kshs_amount;

    // Format phone number
    std::string formatted_phone = mpesa_api_client::format_phone_number(phone_number);

    if (!mpesa_api_client::validate_phone_number(formatted_phone)) {
        return false;
    }

    // Send KES via B2C
    std::string conversation_id = mpesa_client_->send_b2c_payment(
        formatted_phone,
        kes_amount,
        "KSHS Withdrawal",
        "REDEMPTION"
    );

    if (conversation_id.empty()) {
        return false;
    }

    // Will create burn operation when B2C callback confirms success
    // For now, mark as pending
    total_pending_withdrawals_ += kes_amount;

    return true;
}

std::vector<std::string> kakitu_mpesa_bridge::process_bulk_withdrawals(
    std::vector<std::tuple<nano::block_hash, std::string, nano::uint128_t>> const & withdrawals)
{
    std::vector<std::pair<std::string, nano::uint128_t>> payments;

    for (auto const & withdrawal : withdrawals) {
        std::string formatted_phone = mpesa_api_client::format_phone_number(std::get<1>(withdrawal));
        nano::uint128_t kes_amount = std::get<2>(withdrawal);
        payments.emplace_back(formatted_phone, kes_amount);
    }

    return mpesa_client_->send_bulk_b2c(payments);
}

bool kakitu_mpesa_bridge::verify_reserves()
{
    nano::uint128_t mpesa_balance = mpesa_client_->get_account_balance();
    nano::uint128_t kshs_supply = mint_authority_->get_total_supply();

    // Account for pending transactions
    nano::uint128_t adjusted_balance = mpesa_balance + total_pending_deposits_ - total_pending_withdrawals_;

    return adjusted_balance == kshs_supply;
}

kakitu_mpesa_bridge::reserve_status kakitu_mpesa_bridge::get_reserve_status() const
{
    reserve_status status;
    status.mpesa_balance_kes = mpesa_client_->get_account_balance();
    status.kshs_total_supply = mint_authority_->get_total_supply();
    status.pending_deposits = total_pending_deposits_;
    status.pending_withdrawals = total_pending_withdrawals_;

    nano::uint128_t adjusted_balance = status.mpesa_balance_kes +
                                        status.pending_deposits -
                                        status.pending_withdrawals;

    status.is_balanced = (adjusted_balance == status.kshs_total_supply);

    if (!status.kshs_total_supply.is_zero()) {
        auto supply_double = static_cast<double>(status.kshs_total_supply.convert_to<uint64_t>());
        auto balance_double = static_cast<double>(adjusted_balance.convert_to<uint64_t>());
        status.backing_ratio = balance_double / supply_double;
    } else {
        status.backing_ratio = 1.0;
    }

    return status;
}

Json::Value kakitu_mpesa_bridge::generate_reconciliation_report(uint64_t date)
{
    uint64_t start_of_day = date;
    uint64_t end_of_day = date + 86400; // +24 hours

    return mint_authority_->export_audit_report(start_of_day, end_of_day);
}

bool kakitu_mpesa_bridge::link_mpesa_to_kakitu(
    std::string const & mpesa_tx_id,
    nano::account const & kakitu_account)
{
    mpesa_to_kakitu_map_[mpesa_tx_id] = kakitu_account;
    save_transaction_mapping();
    return true;
}

std::vector<mpesa_transaction> kakitu_mpesa_bridge::get_user_transactions(
    nano::account const & kakitu_account)
{
    std::vector<mpesa_transaction> user_txs;

    // Find all M-Pesa transactions linked to this account
    for (auto const & mapping : mpesa_to_kakitu_map_) {
        if (mapping.second == kakitu_account) {
            // TODO: Query full transaction details
        }
    }

    return user_txs;
}

void kakitu_mpesa_bridge::save_transaction_mapping()
{
    // TODO: Persist mappings to disk
}

} // namespace nano
