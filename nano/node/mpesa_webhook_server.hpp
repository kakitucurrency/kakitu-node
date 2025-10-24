#pragma once

#include <json/json.h>
#include <memory>
#include <cstdint>

// Forward declarations
namespace httplib {
    class Server;
}

namespace nano {

// Forward declarations
class kakitu_mpesa_bridge;

/**
 * HTTP webhook server for receiving M-Pesa payment callbacks
 *
 * Handles:
 * - C2B validation requests
 * - C2B confirmation callbacks
 * - STK Push callbacks
 * - B2C result callbacks
 */
class mpesa_webhook_server
{
public:
    /**
     * Constructor
     * @param bridge Shared pointer to the Kakitu M-Pesa bridge
     * @param port Port number to listen on (default: 8080)
     */
    mpesa_webhook_server(
        std::shared_ptr<kakitu_mpesa_bridge> bridge,
        uint16_t port = 8080);

    /**
     * Start the webhook server
     * Runs in a detached thread
     */
    void start();

    /**
     * Stop the webhook server
     */
    void stop();

private:
    /**
     * Setup HTTP routes for the webhook server
     * @param server Reference to the HTTP server
     */
    void setup_routes(httplib::Server & server);

    /**
     * Handle C2B validation request
     * @param request JSON request from Safaricom
     * @return JSON response with validation result
     */
    Json::Value handle_validation(Json::Value const & request);

    /**
     * Handle C2B confirmation callback
     * @param request JSON request from Safaricom
     * @return JSON response acknowledging receipt
     */
    Json::Value handle_confirmation(Json::Value const & request);

    /**
     * Handle STK Push callback
     * @param request JSON request from Safaricom
     * @return JSON response acknowledging receipt
     */
    Json::Value handle_stk_callback(Json::Value const & request);

    /**
     * Handle B2C result callback
     * @param request JSON request from Safaricom
     * @return JSON response acknowledging receipt
     */
    Json::Value handle_b2c_result(Json::Value const & request);

    // Member variables
    std::shared_ptr<kakitu_mpesa_bridge> bridge_;
    uint16_t port_;
    bool running_;
};

} // namespace nano
