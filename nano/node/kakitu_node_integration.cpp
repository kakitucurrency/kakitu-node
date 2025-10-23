/**
 * Kakitu Node Integration Example
 *
 * This file demonstrates how to integrate all Kakitu M-Pesa components
 * into the main Kakitu node.
 */

#include <nano/node/mint_authority.hpp>
#include <nano/node/mpesa_integration.hpp>
#include <nano/node/node.hpp>

#include <fstream>
#include <iostream>
#include <memory>

namespace nano {

class kakitu_node_manager {
public:
    kakitu_node_manager(
        nano::node & node,
        std::string const & config_path,
        std::string const & data_path)
        : node_(node)
        , config_path_(config_path)
        , data_path_(data_path)
    {
    }

    bool initialize()
    {
        // 1. Load configuration
        if (!load_config()) {
            std::cerr << "Failed to load Kakitu configuration" << std::endl;
            return false;
        }

        // 2. Initialize mint authority
        if (!initialize_mint_authority()) {
            std::cerr << "Failed to initialize mint authority" << std::endl;
            return false;
        }

        // 3. Initialize M-Pesa client
        if (!initialize_mpesa_client()) {
            std::cerr << "Failed to initialize M-Pesa client" << std::endl;
            return false;
        }

        // 4. Initialize M-Pesa bridge
        if (!initialize_mpesa_bridge()) {
            std::cerr << "Failed to initialize M-Pesa bridge" << std::endl;
            return false;
        }

        // 5. Start webhook server
        if (config_["webhook_server"]["enabled"].asBool()) {
            if (!start_webhook_server()) {
                std::cerr << "Failed to start webhook server" << std::endl;
                return false;
            }
        }

        std::cout << "Kakitu node initialized successfully" << std::endl;
        return true;
    }

    std::shared_ptr<mint_authority> get_mint_authority() const {
        return mint_authority_;
    }

    std::shared_ptr<kakitu_mpesa_bridge> get_mpesa_bridge() const {
        return mpesa_bridge_;
    }

private:
    bool load_config()
    {
        try {
            std::ifstream config_file(config_path_);
            Json::CharReaderBuilder builder;
            std::string errs;

            if (!Json::parseFromStream(builder, config_file, &config_, &errs)) {
                std::cerr << "Failed to parse config: " << errs << std::endl;
                return false;
            }

            config_file.close();
            return true;
        } catch (std::exception const & e) {
            std::cerr << "Exception loading config: " << e.what() << std::endl;
            return false;
        }
    }

    bool initialize_mint_authority()
    {
        try {
            // Get mint authority private key from environment variable
            std::string private_key_env = config_["kakitu"]["mint_authority"]["private_key_env_var"].asString();
            const char* private_key_hex = std::getenv(private_key_env.c_str());

            if (!private_key_hex) {
                std::cerr << "Mint authority private key not found in environment: " << private_key_env << std::endl;
                return false;
            }

            // Create keypair
            nano::raw_key prv;
            prv.decode_hex(private_key_hex);
            nano::keypair authority_key(std::move(prv));

            // Load or create mint authority
            std::string authority_data_path = data_path_ + "/mint_authority.json";

            std::ifstream test_file(authority_data_path);
            if (test_file.good()) {
                test_file.close();
                // Load existing
                mint_authority_ = mint_authority::load_from_disk(authority_data_path, authority_key);
                std::cout << "Loaded existing mint authority from " << authority_data_path << std::endl;
            } else {
                // Create new
                mint_authority_ = std::make_shared<mint_authority>(authority_key);
                std::cout << "Created new mint authority" << std::endl;
            }

            // Set daily limits if configured
            if (config_["kakitu"].isMember("daily_mint_limit_kes")) {
                nano::uint128_t limit;
                limit.decode_dec(config_["kakitu"]["daily_mint_limit_kes"].asString());
                mint_authority_->set_daily_mint_limit(limit);
            }

            if (config_["kakitu"].isMember("daily_burn_limit_kes")) {
                nano::uint128_t limit;
                limit.decode_dec(config_["kakitu"]["daily_burn_limit_kes"].asString());
                mint_authority_->set_daily_burn_limit(limit);
            }

            return true;
        } catch (std::exception const & e) {
            std::cerr << "Exception initializing mint authority: " << e.what() << std::endl;
            return false;
        }
    }

    bool initialize_mpesa_client()
    {
        try {
            Json::Value mpesa_config = config_["mpesa"];

            std::string consumer_key = mpesa_config["credentials"]["consumer_key"].asString();
            std::string consumer_secret = mpesa_config["credentials"]["consumer_secret"].asString();
            std::string shortcode = mpesa_config["credentials"]["shortcode"].asString();
            std::string passkey = mpesa_config["credentials"]["passkey"].asString();
            bool is_sandbox = mpesa_config["environment"].asString() == "sandbox";

            mpesa_client_ = std::make_shared<mpesa_api_client>(
                consumer_key,
                consumer_secret,
                shortcode,
                passkey,
                is_sandbox
            );

            // Test authentication
            if (!mpesa_client_->authenticate()) {
                std::cerr << "Failed to authenticate with M-Pesa API" << std::endl;
                return false;
            }

            std::cout << "M-Pesa API client initialized and authenticated" << std::endl;

            // Register C2B URLs
            if (mpesa_config["webhook_urls"].isMember("confirmation_url")) {
                std::string validation_url = mpesa_config["webhook_urls"]["validation_url"].asString();
                std::string confirmation_url = mpesa_config["webhook_urls"]["confirmation_url"].asString();

                bool registered = mpesa_client_->register_c2b_urls(validation_url, confirmation_url);
                if (registered) {
                    std::cout << "C2B URLs registered successfully" << std::endl;
                } else {
                    std::cerr << "Warning: C2B URL registration may have failed (check if already registered)" << std::endl;
                }
            }

            return true;
        } catch (std::exception const & e) {
            std::cerr << "Exception initializing M-Pesa client: " << e.what() << std::endl;
            return false;
        }
    }

    bool initialize_mpesa_bridge()
    {
        try {
            mpesa_bridge_ = std::make_shared<kakitu_mpesa_bridge>(mpesa_client_, mint_authority_);

            std::cout << "M-Pesa bridge initialized" << std::endl;
            return true;
        } catch (std::exception const & e) {
            std::cerr << "Exception initializing M-Pesa bridge: " << e.what() << std::endl;
            return false;
        }
    }

    bool start_webhook_server()
    {
        try {
            uint16_t port = config_["webhook_server"]["port"].asUInt();

            webhook_server_ = std::make_shared<mpesa_webhook_server>(mpesa_bridge_, port);
            webhook_server_->start();

            std::cout << "Webhook server started on port " << port << std::endl;
            return true;
        } catch (std::exception const & e) {
            std::cerr << "Exception starting webhook server: " << e.what() << std::endl;
            return false;
        }
    }

    nano::node & node_;
    std::string config_path_;
    std::string data_path_;

    Json::Value config_;
    std::shared_ptr<mint_authority> mint_authority_;
    std::shared_ptr<mpesa_api_client> mpesa_client_;
    std::shared_ptr<kakitu_mpesa_bridge> mpesa_bridge_;
    std::shared_ptr<mpesa_webhook_server> webhook_server_;
};

} // namespace nano

// Example usage in main():
/*
int main(int argc, char** argv)
{
    // ... existing Nano node initialization ...

    // Initialize Kakitu components
    nano::kakitu_node_manager kakitu_manager(
        node,
        "/etc/kakitu/mpesa_config.json",
        "/var/kakitu/data"
    );

    if (!kakitu_manager.initialize()) {
        std::cerr << "Failed to initialize Kakitu components" << std::endl;
        return 1;
    }

    // The node is now running with M-Pesa integration
    // Webhooks will handle deposits automatically
    // RPC endpoints are available for querying supply, reserves, etc.

    // ... rest of node operation ...

    return 0;
}
*/
