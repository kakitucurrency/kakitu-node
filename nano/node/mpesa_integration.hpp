#pragma once

#include <nano/lib/numbers.hpp>
#include <nano/node/mint_authority.hpp>
#include <string>
#include <map>
#include <functional>
#include <curl/curl.h>
#include <json/json.h>

namespace nano {

/**
 * M-Pesa API Integration for Kakitu
 * Handles deposits (C2B via Paybill) and withdrawals (B2C bulk disbursement)
 */

// M-Pesa transaction types
enum class mpesa_transaction_type {
    C2B_DEPOSIT,      // Customer deposits KES to buy KSHS
    B2C_WITHDRAWAL,   // Business sends KES when user sells KSHS
    B2B_TRANSFER,     // Business to business (for float management)
    REVERSAL          // Transaction reversal
};

// M-Pesa transaction status
enum class mpesa_status {
    PENDING,          // Waiting for confirmation
    COMPLETED,        // Successfully processed
    FAILED,           // Transaction failed
    REVERSED,         // Transaction was reversed
    TIMEOUT           // Transaction timed out
};

struct mpesa_transaction {
    std::string transaction_id;           // M-Pesa transaction ID (e.g., "QGK1234567")
    std::string phone_number;             // User's phone (254712345678)
    nano::uint128_t amount_kes;           // Amount in KES
    mpesa_transaction_type type;
    mpesa_status status;
    uint64_t timestamp;
    std::string first_name;               // From M-Pesa
    std::string last_name;                // From M-Pesa
    std::string business_short_code;      // Your paybill number
    std::string bill_ref_number;          // User's account/reference

    // For linking to Kakitu account
    nano::account kakitu_account;         // User's KSHS address
    nano::uint128_t kshs_amount;          // KSHS tokens minted/burned

    // Cryptographic proof
    nano::block_hash proof_hash;          // Hash of this transaction
    nano::signature mint_signature;       // Signature from mint authority

    // Calculate hash for tamper detection
    nano::block_hash calculate_hash() const;
};

class mpesa_api_client {
public:
    mpesa_api_client(
        std::string const & consumer_key,
        std::string const & consumer_secret,
        std::string const & shortcode,
        std::string const & passkey,
        bool is_sandbox = false
    );

    // ============================================
    // DEPOSIT FLOW (User buys KSHS with KES)
    // ============================================

    /**
     * Register C2B validation and confirmation URLs
     * This tells M-Pesa where to send payment notifications
     */
    bool register_c2b_urls(
        std::string const & validation_url,
        std::string const & confirmation_url
    );

    /**
     * Process incoming C2B callback from M-Pesa
     * This is called when user sends money to your paybill
     */
    mpesa_transaction process_c2b_callback(Json::Value const & callback_data);

    /**
     * STK Push - Prompt user to enter M-Pesa PIN
     * Alternative to user manually sending money
     */
    std::string initiate_stk_push(
        std::string const & phone_number,
        nano::uint128_t amount_kes,
        std::string const & account_reference,
        std::string const & description
    );

    /**
     * Check STK Push status
     */
    mpesa_status query_stk_status(std::string const & checkout_request_id);

    // ============================================
    // WITHDRAWAL FLOW (User sells KSHS for KES)
    // ============================================

    /**
     * B2C Payment - Send KES to user's phone
     */
    std::string send_b2c_payment(
        std::string const & phone_number,
        nano::uint128_t amount_kes,
        std::string const & remarks,
        std::string const & occasion = "KSHS_REDEMPTION"
    );

    /**
     * Check B2C transaction status
     */
    mpesa_status query_b2c_status(std::string const & transaction_id);

    /**
     * Bulk disbursement - Send to multiple users at once
     */
    std::vector<std::string> send_bulk_b2c(
        std::vector<std::pair<std::string, nano::uint128_t>> const & payments
    );

    // ============================================
    // ACCOUNT MANAGEMENT
    // ============================================

    /**
     * Check M-Pesa account balance (reserves)
     */
    nano::uint128_t get_account_balance();

    /**
     * Query transaction by M-Pesa ID
     */
    mpesa_transaction query_transaction(std::string const & transaction_id);

    /**
     * Get transaction history
     */
    std::vector<mpesa_transaction> get_transaction_history(
        uint64_t start_timestamp,
        uint64_t end_timestamp
    );

    // ============================================
    // UTILITIES
    // ============================================

    /**
     * Validate phone number format
     */
    static bool validate_phone_number(std::string const & phone);

    /**
     * Format phone to 254XXXXXXXXX
     */
    static std::string format_phone_number(std::string const & phone);

private:
    std::string consumer_key_;
    std::string consumer_secret_;
    std::string shortcode_;
    std::string passkey_;
    std::string base_url_;
    std::string access_token_;
    uint64_t token_expiry_;

    // HTTP client
    CURL* curl_handle_;

    // Get OAuth token
    bool authenticate();

    // Make API request
    Json::Value make_request(
        std::string const & endpoint,
        std::string const & method,
        Json::Value const & payload
    );

    // Generate security credential
    std::string generate_password() const;
    std::string generate_timestamp() const;
};

/**
 * Integration layer between M-Pesa and Kakitu mint system
 */
class kakitu_mpesa_bridge {
public:
    kakitu_mpesa_bridge(
        std::shared_ptr<mpesa_api_client> mpesa_client,
        std::shared_ptr<nano::mint_authority> mint_authority
    );

    // ============================================
    // DEPOSIT: KES → KSHS
    // ============================================

    /**
     * Process deposit when M-Pesa payment received
     * 1. Verify M-Pesa transaction
     * 2. Mint equivalent KSHS
     * 3. Send KSHS to user's wallet
     * 4. Log operation with proof
     */
    bool process_deposit(
        mpesa_transaction const & mpesa_tx,
        nano::account const & destination_account
    );

    /**
     * Initiate deposit via STK Push
     * Returns checkout request ID to track
     */
    std::string initiate_deposit(
        std::string const & phone_number,
        nano::account const & kakitu_account,
        nano::uint128_t amount_kes
    );

    // ============================================
    // WITHDRAWAL: KSHS → KES
    // ============================================

    /**
     * Process withdrawal when user burns KSHS
     * 1. Verify KSHS burned to burn address
     * 2. Send equivalent KES via M-Pesa B2C
     * 3. Log operation with proof
     */
    bool process_withdrawal(
        nano::block_hash const & burn_block_hash,
        std::string const & phone_number,
        nano::uint128_t kshs_amount
    );

    /**
     * Batch process multiple withdrawals
     * More efficient for bulk operations
     */
    std::vector<std::string> process_bulk_withdrawals(
        std::vector<std::tuple<nano::block_hash, std::string, nano::uint128_t>> const & withdrawals
    );

    // ============================================
    // VERIFICATION & RECONCILIATION
    // ============================================

    /**
     * Verify reserves match supply
     * Total KSHS supply == M-Pesa account balance
     */
    bool verify_reserves();

    /**
     * Get reserve status
     */
    struct reserve_status {
        nano::uint128_t mpesa_balance_kes;
        nano::uint128_t kshs_total_supply;
        nano::uint128_t pending_deposits;
        nano::uint128_t pending_withdrawals;
        bool is_balanced;
        double backing_ratio;  // Should always be 1.0
    };

    reserve_status get_reserve_status() const;

    /**
     * Daily reconciliation report
     */
    Json::Value generate_reconciliation_report(uint64_t date);

    // ============================================
    // TRANSACTION TRACKING
    // ============================================

    /**
     * Link M-Pesa transaction to Kakitu account
     */
    bool link_mpesa_to_kakitu(
        std::string const & mpesa_tx_id,
        nano::account const & kakitu_account
    );

    /**
     * Get user's transaction history
     */
    std::vector<mpesa_transaction> get_user_transactions(
        nano::account const & kakitu_account
    );

    /**
     * Save transaction mapping to database
     */
    void save_transaction_mapping();

private:
    std::shared_ptr<mpesa_api_client> mpesa_client_;
    std::shared_ptr<nano::mint_authority> mint_authority_;

    // Transaction mapping: M-Pesa ID → Kakitu account
    std::map<std::string, nano::account> mpesa_to_kakitu_map_;

    // Pending transactions awaiting confirmation
    std::map<std::string, mpesa_transaction> pending_transactions_;

    // For reserve tracking
    nano::uint128_t total_pending_deposits_{0};
    nano::uint128_t total_pending_withdrawals_{0};

    // Callbacks
    std::function<void(mpesa_transaction const &)> on_deposit_callback_;
    std::function<void(mpesa_transaction const &)> on_withdrawal_callback_;
};

/**
 * Webhook handler for M-Pesa callbacks
 * Run this as HTTP server to receive M-Pesa notifications
 */
class mpesa_webhook_server {
public:
    mpesa_webhook_server(
        std::shared_ptr<kakitu_mpesa_bridge> bridge,
        uint16_t port = 8080
    );

    /**
     * Start webhook server
     */
    void start();

    /**
     * Stop webhook server
     */
    void stop();

    /**
     * Handle C2B validation callback
     * M-Pesa calls this to check if transaction should proceed
     */
    Json::Value handle_validation(Json::Value const & request);

    /**
     * Handle C2B confirmation callback
     * M-Pesa calls this when payment completed
     */
    Json::Value handle_confirmation(Json::Value const & request);

    /**
     * Handle STK Push callback
     */
    Json::Value handle_stk_callback(Json::Value const & request);

    /**
     * Handle B2C result callback
     */
    Json::Value handle_b2c_result(Json::Value const & request);

private:
    std::shared_ptr<kakitu_mpesa_bridge> bridge_;
    uint16_t port_;
    bool running_;

    // HTTP server implementation (using something like cpp-httplib)
    void setup_routes();
};

} // namespace nano
