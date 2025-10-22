#pragma once

#include <nano/lib/numbers.hpp>
#include <nano/lib/blocks.hpp>
#include <nano/secure/common.hpp>
#include <vector>
#include <string>
#include <chrono>
#include <json/json.h>

namespace nano {

/**
 * Mint/Burn operation types
 */
enum class mint_operation_type {
    MINT,    // Creating new KSHS tokens (user deposited KES via M-Pesa)
    BURN,    // Destroying KSHS tokens (user withdrew KES via M-Pesa)
    AUDIT    // Regular reserve verification/audit operation
};

/**
 * Complete record of a mint or burn operation
 * This provides full cryptographic proof and auditability
 */
struct mint_operation {
    // Operation details
    mint_operation_type type;
    nano::uint128_t kshs_amount;          // Amount of KSHS tokens
    nano::uint128_t fiat_amount_kes;      // Equivalent KES amount

    // Accounts
    nano::account destination;             // For MINT: where tokens go
    nano::account source;                  // For BURN: where tokens came from
    nano::account burn_address;            // Burn address used

    // M-Pesa integration
    std::string mpesa_transaction_id;      // M-Pesa receipt (e.g., "QGK1234567")
    std::string phone_number;              // User's M-Pesa number
    std::string mpesa_first_name;          // From M-Pesa
    std::string mpesa_last_name;           // From M-Pesa

    // Timestamps
    uint64_t timestamp;                    // Unix timestamp of operation
    uint64_t mpesa_timestamp;              // M-Pesa transaction timestamp

    // Cryptographic proof
    nano::block_hash operation_hash;       // Hash of entire operation
    nano::block_hash block_hash;           // Kakitu block hash (mint/burn)
    nano::signature signature;             // Signed by mint authority

    // Merkle proof for log integrity
    nano::block_hash merkle_root;          // Merkle root after this operation
    uint64_t log_index;                    // Position in operation log

    // Metadata
    std::string notes;                     // Optional notes
    std::string api_response;              // M-Pesa API response (for debugging)

    /**
     * Calculate cryptographic hash of this operation
     * Includes all fields to prevent tampering
     */
    nano::block_hash calculate_hash() const;

    /**
     * Verify mint authority signature
     */
    bool verify_signature(nano::public_key const & mint_authority_key) const;

    /**
     * Serialize to JSON for logging
     */
    Json::Value to_json() const;

    /**
     * Deserialize from JSON
     */
    static mint_operation from_json(Json::Value const & json);
};

/**
 * Mint Authority - Controls creation and destruction of KSHS tokens
 * This is the central authority that maintains 1:1 KES backing
 */
class mint_authority {
public:
    /**
     * Initialize mint authority with keypair
     */
    explicit mint_authority(nano::keypair const & authority_key);

    /**
     * Load existing mint authority from disk
     */
    static std::shared_ptr<mint_authority> load_from_disk(
        std::string const & data_path,
        nano::keypair const & authority_key
    );

    // ============================================
    // CORE OPERATIONS
    // ============================================

    /**
     * Create a MINT operation
     * Called when user deposits KES via M-Pesa
     *
     * @param destination Kakitu account to receive KSHS
     * @param kshs_amount Amount of KSHS to mint
     * @param mpesa_tx_id M-Pesa transaction ID
     * @param fiat_amount_kes KES amount deposited
     * @param phone_number User's M-Pesa phone
     * @return Signed mint operation
     */
    mint_operation create_mint(
        nano::account const & destination,
        nano::uint128_t kshs_amount,
        std::string const & mpesa_tx_id,
        nano::uint128_t fiat_amount_kes,
        std::string const & phone_number,
        std::string const & mpesa_first_name = "",
        std::string const & mpesa_last_name = ""
    );

    /**
     * Create a BURN operation
     * Called when user withdraws KES via M-Pesa
     *
     * @param source Kakitu account that sent KSHS to burn address
     * @param kshs_amount Amount of KSHS burned
     * @param mpesa_tx_id M-Pesa transaction ID for withdrawal
     * @param fiat_amount_kes KES amount sent to user
     * @param phone_number User's M-Pesa phone
     * @return Signed burn operation
     */
    mint_operation create_burn(
        nano::account const & source,
        nano::uint128_t kshs_amount,
        std::string const & mpesa_tx_id,
        nano::uint128_t fiat_amount_kes,
        std::string const & phone_number
    );

    /**
     * Create an AUDIT operation
     * Records periodic verification of reserves
     */
    mint_operation create_audit(
        nano::uint128_t mpesa_balance,
        nano::uint128_t kshs_supply,
        std::string const & notes = ""
    );

    // ============================================
    // SUPPLY TRACKING
    // ============================================

    /**
     * Get total KSHS supply (should equal M-Pesa reserves)
     */
    nano::uint128_t get_total_supply() const {
        return total_kshs_supply_;
    }

    /**
     * Get total KES reserves (from M-Pesa)
     */
    nano::uint128_t get_total_reserves() const {
        return total_kes_reserves_;
    }

    /**
     * Check if reserves are balanced (supply == reserves)
     */
    bool verify_reserves() const {
        return total_kshs_supply_ == total_kes_reserves_;
    }

    /**
     * Get backing ratio (should always be 1.0)
     */
    double get_backing_ratio() const;

    // ============================================
    // OPERATION LOG
    // ============================================

    /**
     * Get all operations
     */
    std::vector<mint_operation> const & get_operation_log() const {
        return operation_log_;
    }

    /**
     * Get operations by type
     */
    std::vector<mint_operation> get_operations_by_type(
        mint_operation_type type
    ) const;

    /**
     * Get operations in time range
     */
    std::vector<mint_operation> get_operations_by_timerange(
        uint64_t start_timestamp,
        uint64_t end_timestamp
    ) const;

    /**
     * Find operation by M-Pesa transaction ID
     */
    std::optional<mint_operation> find_by_mpesa_id(
        std::string const & mpesa_tx_id
    ) const;

    /**
     * Find operation by Kakitu block hash
     */
    std::optional<mint_operation> find_by_block_hash(
        nano::block_hash const & hash
    ) const;

    // ============================================
    // PERSISTENCE
    // ============================================

    /**
     * Save operation log to disk
     */
    bool save_to_disk(std::string const & data_path);

    /**
     * Load operation log from disk
     */
    bool load_from_disk(std::string const & data_path);

    /**
     * Export audit report as JSON
     */
    Json::Value export_audit_report(
        uint64_t start_timestamp,
        uint64_t end_timestamp
    ) const;

    /**
     * Export to CSV for accounting
     */
    std::string export_to_csv(
        uint64_t start_timestamp,
        uint64_t end_timestamp
    ) const;

    // ============================================
    // VERIFICATION
    // ============================================

    /**
     * Verify entire operation log integrity
     * Checks Merkle tree and signatures
     */
    bool verify_log_integrity() const;

    /**
     * Verify a specific operation
     */
    bool verify_operation(mint_operation const & op) const;

    /**
     * Get public key for verification
     */
    nano::public_key get_public_key() const {
        return authority_key_.pub;
    }

    // ============================================
    // RATE LIMITING & SECURITY
    // ============================================

    /**
     * Set daily mint limit
     */
    void set_daily_mint_limit(nano::uint128_t limit) {
        daily_mint_limit_ = limit;
    }

    /**
     * Set daily burn limit
     */
    void set_daily_burn_limit(nano::uint128_t limit) {
        daily_burn_limit_ = limit;
    }

    /**
     * Check if mint is within daily limit
     */
    bool is_within_mint_limit(nano::uint128_t amount) const;

    /**
     * Check if burn is within daily limit
     */
    bool is_within_burn_limit(nano::uint128_t amount) const;

    /**
     * Reset daily counters (call at midnight)
     */
    void reset_daily_limits();

private:
    // Mint authority keypair
    nano::keypair authority_key_;

    // Supply tracking
    nano::uint128_t total_kshs_supply_{0};
    nano::uint128_t total_kes_reserves_{0};

    // Operation log (immutable append-only)
    std::vector<mint_operation> operation_log_;

    // Merkle tree for log integrity
    std::vector<nano::block_hash> merkle_tree_;

    // Rate limiting
    nano::uint128_t daily_mint_limit_{0};  // 0 = no limit
    nano::uint128_t daily_burn_limit_{0};  // 0 = no limit
    nano::uint128_t daily_minted_today_{0};
    nano::uint128_t daily_burned_today_{0};
    uint64_t last_reset_timestamp_;

    // Internal helpers
    nano::signature sign_operation(mint_operation const & op) const;
    nano::block_hash calculate_merkle_root();
    void update_merkle_tree(nano::block_hash const & operation_hash);
    void check_and_reset_daily_limits();
};

/**
 * Mint operation validator
 * Used by nodes to verify mint/burn operations
 */
class mint_operation_validator {
public:
    explicit mint_operation_validator(nano::public_key const & mint_authority_key);

    /**
     * Validate a mint operation
     */
    bool validate(mint_operation const & op) const;

    /**
     * Validate signature
     */
    bool validate_signature(mint_operation const & op) const;

    /**
     * Validate hash
     */
    bool validate_hash(mint_operation const & op) const;

private:
    nano::public_key mint_authority_key_;
};

} // namespace nano
