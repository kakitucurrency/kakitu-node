/**
 * Test Program for Kakitu M-Pesa Integration
 *
 * This standalone program tests the M-Pesa integration without needing
 * the full node to be running. Use this to verify STK Push, webhooks, and minting.
 *
 * Compile:
 *   g++ -std=c++17 test_kakitu_mpesa.cpp nano/node/mpesa_integration.cpp nano/node/mint_authority.cpp \
 *       -I. -I./nano -lcurl -lssl -lcrypto -ljsoncpp -o test_kakitu
 *
 * Run:
 *   ./test_kakitu nano/node/mpesa_config_sandbox.json
 */

#include <nano/node/mpesa_integration.hpp>
#include <nano/node/mint_authority.hpp>
#include <iostream>
#include <fstream>
#include <memory>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <config_file.json>" << std::endl;
    std::cout << "Example: " << program_name << " nano/node/mpesa_config_sandbox.json" << std::endl;
}

void test_authentication(std::shared_ptr<nano::mpesa_api_client> client) {
    std::cout << "\n=== Testing M-Pesa Authentication ===" << std::endl;

    if (client->authenticate()) {
        std::cout << "✓ Authentication successful!" << std::endl;
    } else {
        std::cout << "✗ Authentication failed!" << std::endl;
    }
}

void test_stk_push(std::shared_ptr<nano::mpesa_api_client> client, Json::Value const & config) {
    std::cout << "\n=== Testing STK Push ===" << std::endl;

    std::string phone = config["mpesa"]["test_credentials"]["test_phone_number"].asString();
    std::string amount_str = config["mpesa"]["test_credentials"].get("test_amount", "1").asString();

    nano::uint128_t amount;
    amount.decode_dec(amount_str);

    std::cout << "Initiating STK Push to: " << phone << " for KES " << amount_str << std::endl;

    std::string checkout_id = client->initiate_stk_push(
        phone,
        amount,
        "TEST_KSHS_DEPOSIT",
        "Test Kakitu deposit"
    );

    if (!checkout_id.empty()) {
        std::cout << "✓ STK Push initiated successfully!" << std::endl;
        std::cout << "  Checkout Request ID: " << checkout_id << std::endl;
        std::cout << "  User should receive STK Push prompt on phone: " << phone << std::endl;
    } else {
        std::cout << "✗ STK Push failed!" << std::endl;
    }
}

void test_mint_authority() {
    std::cout << "\n=== Testing Mint Authority ===" << std::endl;

    // Generate test keypair
    nano::keypair mint_key;

    std::cout << "Generated mint authority keypair:" << std::endl;
    std::cout << "  Public key:  " << mint_key.pub.to_account() << std::endl;
    std::cout << "  Private key: " << mint_key.prv.to_string() << std::endl;
    std::cout << "  (Save private key in env var: KAKITU_MINT_PRIVATE_KEY)" << std::endl;

    // Create mint authority
    auto mint_authority = std::make_shared<nano::mint_authority>(mint_key);

    // Create test account
    nano::keypair user_key;
    std::cout << "\nGenerated test user account:" << std::endl;
    std::cout << "  Account: " << user_key.pub.to_account() << std::endl;

    // Test mint operation
    nano::uint128_t amount;
    amount.decode_dec("100");

    std::cout << "\nCreating test mint operation..." << std::endl;
    auto mint_op = mint_authority->create_mint(
        user_key.pub,
        amount,
        "TEST_MPESA_TX_12345",
        amount,
        "254712345678"
    );

    std::cout << "✓ Mint operation created!" << std::endl;
    std::cout << "  Operation hash: " << mint_op.operation_hash.to_string() << std::endl;
    std::cout << "  KSHS amount: " << mint_op.kshs_amount.to_string_dec() << std::endl;
    std::cout << "  Destination: " << mint_op.destination.to_account() << std::endl;

    // Verify operation
    bool valid = mint_authority->verify_operation(mint_op);
    std::cout << "  Signature valid: " << (valid ? "✓" : "✗") << std::endl;

    // Check supply
    std::cout << "\nCurrent supply:" << std::endl;
    std::cout << "  Total KSHS: " << mint_authority->get_total_supply().to_string_dec() << std::endl;
    std::cout << "  Total KES:  " << mint_authority->get_total_reserves().to_string_dec() << std::endl;
    std::cout << "  Backing ratio: " << mint_authority->get_backing_ratio() << std::endl;
}

void test_full_flow(std::shared_ptr<nano::mpesa_api_client> client, Json::Value const & config) {
    std::cout << "\n=== Testing Full Deposit Flow ===" << std::endl;

    // Create mint authority
    nano::keypair mint_key;
    auto mint_authority = std::make_shared<nano::mint_authority>(mint_key);

    // Create bridge
    auto bridge = std::make_shared<nano::kakitu_mpesa_bridge>(client, mint_authority);

    // Create user account
    nano::keypair user_key;
    std::cout << "User account: " << user_key.pub.to_account() << std::endl;

    // Initiate deposit
    std::string phone = config["mpesa"]["test_credentials"]["test_phone_number"].asString();
    nano::uint128_t amount;
    amount.decode_dec("10");

    std::cout << "Initiating deposit of KES 10 for user..." << std::endl;
    std::string checkout_id = bridge->initiate_deposit(phone, user_key.pub, amount);

    if (!checkout_id.empty()) {
        std::cout << "✓ Deposit initiated!" << std::endl;
        std::cout << "  Checkout ID: " << checkout_id << std::endl;
        std::cout << "\nNext steps:" << std::endl;
        std::cout << "  1. User receives STK Push on phone " << phone << std::endl;
        std::cout << "  2. User enters M-Pesa PIN" << std::endl;
        std::cout << "  3. M-Pesa sends callback to webhook server" << std::endl;
        std::cout << "  4. Webhook processes payment and mints KSHS" << std::endl;
        std::cout << "  5. User receives 10 KSHS tokens at: " << user_key.pub.to_account() << std::endl;
    } else {
        std::cout << "✗ Deposit initiation failed!" << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string config_path = argv[1];

    std::cout << "========================================" << std::endl;
    std::cout << "  Kakitu M-Pesa Integration Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Config file: " << config_path << std::endl;

    // Load configuration
    Json::Value config;
    try {
        std::ifstream config_file(config_path);
        if (!config_file.good()) {
            std::cerr << "Error: Could not open config file: " << config_path << std::endl;
            return 1;
        }

        Json::CharReaderBuilder builder;
        std::string errs;

        if (!Json::parseFromStream(builder, config_file, &config, &errs)) {
            std::cerr << "Error parsing JSON: " << errs << std::endl;
            return 1;
        }

        config_file.close();
        std::cout << "✓ Configuration loaded successfully" << std::endl;
    } catch (std::exception const & e) {
        std::cerr << "Exception loading config: " << e.what() << std::endl;
        return 1;
    }

    // Create M-Pesa client
    auto mpesa_client = std::make_shared<nano::mpesa_api_client>(config["mpesa"]);

    // Run tests
    test_authentication(mpesa_client);
    test_stk_push(mpesa_client, config);
    test_mint_authority();
    test_full_flow(mpesa_client, config);

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Testing Complete!" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\nNote: For full integration testing:" << std::endl;
    std::cout << "  1. Run the webhook server (see mpesa_webhook_server.cpp)" << std::endl;
    std::cout << "  2. Expose it via ngrok or similar: ngrok http 8080" << std::endl;
    std::cout << "  3. Update webhook_urls in config with your ngrok URL" << std::endl;
    std::cout << "  4. Run this test again to trigger real STK Push" << std::endl;
    std::cout << "  5. Enter PIN on test phone to complete transaction" << std::endl;

    return 0;
}
