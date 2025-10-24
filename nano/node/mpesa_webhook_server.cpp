#include <nano/node/mpesa_integration.hpp>
#include <nano/node/mint_authority.hpp>

#include <httplib.h>
#include <json/json.h>

#include <thread>
#include <iostream>
#include <fstream>

namespace nano {

// ============================================
// mpesa_webhook_server Implementation
// ============================================

mpesa_webhook_server::mpesa_webhook_server(
    std::shared_ptr<kakitu_mpesa_bridge> bridge,
    uint16_t port)
    : bridge_(bridge)
    , port_(port)
    , running_(false)
{
}

void mpesa_webhook_server::start()
{
    if (running_) {
        return;
    }

    running_ = true;

    // Start HTTP server in a separate thread
    std::thread server_thread([this]() {
        httplib::Server server;

        setup_routes(server);

        std::cout << "M-Pesa Webhook Server starting on port " << port_ << std::endl;
        server.listen("0.0.0.0", port_);
    });

    server_thread.detach();
}

void mpesa_webhook_server::stop()
{
    running_ = false;
}

void mpesa_webhook_server::setup_routes(httplib::Server & server)
{
    // Health check endpoint
    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        Json::Value response;
        response["status"] = "healthy";
        response["service"] = "Kakitu M-Pesa Webhook";

        Json::StreamWriterBuilder writer;
        res.set_content(Json::writeString(writer, response), "application/json");
    });

    // C2B Validation callback
    server.Post("/mpesa/validate", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            Json::Value request;
            Json::CharReaderBuilder builder;
            std::istringstream s(req.body);
            std::string errs;

            if (!Json::parseFromStream(builder, s, &request, &errs)) {
                res.status = 400;
                res.set_content("{\"ResultCode\": 1, \"ResultDesc\": \"Invalid JSON\"}", "application/json");
                return;
            }

            Json::Value response = handle_validation(request);

            Json::StreamWriterBuilder writer;
            res.set_content(Json::writeString(writer, response), "application/json");
        } catch (std::exception const & e) {
            res.status = 500;
            Json::Value error;
            error["ResultCode"] = 1;
            error["ResultDesc"] = std::string("Error: ") + e.what();

            Json::StreamWriterBuilder writer;
            res.set_content(Json::writeString(writer, error), "application/json");
        }
    });

    // C2B Confirmation callback
    server.Post("/mpesa/confirm", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            Json::Value request;
            Json::CharReaderBuilder builder;
            std::istringstream s(req.body);
            std::string errs;

            if (!Json::parseFromStream(builder, s, &request, &errs)) {
                res.status = 400;
                res.set_content("{\"ResultCode\": 1, \"ResultDesc\": \"Invalid JSON\"}", "application/json");
                return;
            }

            Json::Value response = handle_confirmation(request);

            Json::StreamWriterBuilder writer;
            res.set_content(Json::writeString(writer, response), "application/json");
        } catch (std::exception const & e) {
            res.status = 500;
            Json::Value error;
            error["ResultCode"] = 1;
            error["ResultDesc"] = std::string("Error: ") + e.what();

            Json::StreamWriterBuilder writer;
            res.set_content(Json::writeString(writer, error), "application/json");
        }
    });

    // STK Push callback
    server.Post("/mpesa/stk-callback", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            Json::Value request;
            Json::CharReaderBuilder builder;
            std::istringstream s(req.body);
            std::string errs;

            if (!Json::parseFromStream(builder, s, &request, &errs)) {
                res.status = 400;
                res.set_content("{\"ResultCode\": 1, \"ResultDesc\": \"Invalid JSON\"}", "application/json");
                return;
            }

            Json::Value response = handle_stk_callback(request);

            Json::StreamWriterBuilder writer;
            res.set_content(Json::writeString(writer, response), "application/json");
        } catch (std::exception const & e) {
            res.status = 500;
            Json::Value error;
            error["ResultCode"] = 1;
            error["ResultDesc"] = std::string("Error: ") + e.what();

            Json::StreamWriterBuilder writer;
            res.set_content(Json::writeString(writer, error), "application/json");
        }
    });

    // B2C Result callback
    server.Post("/mpesa/b2c-result", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            Json::Value request;
            Json::CharReaderBuilder builder;
            std::istringstream s(req.body);
            std::string errs;

            if (!Json::parseFromStream(builder, s, &request, &errs)) {
                res.status = 400;
                res.set_content("{\"ResultCode\": 1, \"ResultDesc\": \"Invalid JSON\"}", "application/json");
                return;
            }

            Json::Value response = handle_b2c_result(request);

            Json::StreamWriterBuilder writer;
            res.set_content(Json::writeString(writer, response), "application/json");
        } catch (std::exception const & e) {
            res.status = 500;
            Json::Value error;
            error["ResultCode"] = 1;
            error["ResultDesc"] = std::string("Error: ") + e.what();

            Json::StreamWriterBuilder writer;
            res.set_content(Json::writeString(writer, error), "application/json");
        }
    });
}

Json::Value mpesa_webhook_server::handle_validation(Json::Value const & request)
{
    // Log the validation request
    std::cout << "Validation request received" << std::endl;
    std::cout << request.toStyledString() << std::endl;

    // In production, you would:
    // 1. Verify the request is from Safaricom (IP whitelist)
    // 2. Check transaction amount is within limits
    // 3. Validate account reference (Kakitu address)
    // 4. Check for duplicate transactions

    // For now, auto-approve all transactions
    Json::Value response;
    response["ResultCode"] = 0;
    response["ResultDesc"] = "Accepted";

    return response;
}

Json::Value mpesa_webhook_server::handle_confirmation(Json::Value const & request)
{
    std::cout << "Confirmation received" << std::endl;
    std::cout << request.toStyledString() << std::endl;

    try {
        // Parse M-Pesa transaction
        mpesa_transaction mpesa_tx;
        mpesa_tx.transaction_id = request.get("TransID", "").asString();
        mpesa_tx.phone_number = request.get("MSISDN", "").asString();
        mpesa_tx.first_name = request.get("FirstName", "").asString();
        mpesa_tx.last_name = request.get("LastName", "").asString();
        mpesa_tx.business_short_code = request.get("BusinessShortCode", "").asString();
        mpesa_tx.bill_ref_number = request.get("BillRefNumber", "").asString();

        std::string amount_str = request.get("TransAmount", "0").asString();
        mpesa_tx.amount_kes = nano::uint128_t(amount_str);

        mpesa_tx.type = mpesa_transaction_type::C2B_DEPOSIT;
        mpesa_tx.status = mpesa_status::COMPLETED;
        mpesa_tx.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;

        // Get destination account from BillRefNumber (should be kshs_ address)
        nano::account destination;
        auto error = destination.decode_account(mpesa_tx.bill_ref_number);

        if (error) {
            std::cerr << "Invalid Kakitu address in BillRefNumber: " << mpesa_tx.bill_ref_number << std::endl;

            Json::Value response;
            response["ResultCode"] = 1;
            response["ResultDesc"] = "Invalid account reference";
            return response;
        }

        // Process the deposit
        bool success = bridge_->process_deposit(mpesa_tx, destination);

        if (success) {
            std::cout << "Deposit processed successfully: " << mpesa_tx.transaction_id << std::endl;

            Json::Value response;
            response["ResultCode"] = 0;
            response["ResultDesc"] = "Success";
            return response;
        } else {
            std::cerr << "Failed to process deposit: " << mpesa_tx.transaction_id << std::endl;

            Json::Value response;
            response["ResultCode"] = 1;
            response["ResultDesc"] = "Processing failed";
            return response;
        }
    } catch (std::exception const & e) {
        std::cerr << "Exception in handle_confirmation: " << e.what() << std::endl;

        Json::Value response;
        response["ResultCode"] = 1;
        response["ResultDesc"] = std::string("Error: ") + e.what();
        return response;
    }
}

Json::Value mpesa_webhook_server::handle_stk_callback(Json::Value const & request)
{
    std::cout << "STK Push callback received" << std::endl;
    std::cout << request.toStyledString() << std::endl;

    try {
        Json::Value body = request["Body"];
        Json::Value stk_callback = body["stkCallback"];

        int result_code = stk_callback.get("ResultCode", 1).asInt();
        std::string checkout_id = stk_callback.get("CheckoutRequestID", "").asString();

        if (result_code == 0) {
            // Payment successful
            Json::Value callback_metadata = stk_callback["CallbackMetadata"];
            Json::Value items = callback_metadata["Item"];

            std::string mpesa_receipt;
            std::string phone_number;
            std::string amount_str;

            for (auto const & item : items) {
                std::string name = item.get("Name", "").asString();

                if (name == "MpesaReceiptNumber") {
                    mpesa_receipt = item.get("Value", "").asString();
                } else if (name == "PhoneNumber") {
                    phone_number = std::to_string(item.get("Value", 0).asUInt64());
                } else if (name == "Amount") {
                    amount_str = std::to_string(item.get("Value", 0).asUInt());
                }
            }

            std::cout << "STK Payment successful: " << mpesa_receipt << std::endl;

            // TODO: Process the deposit using checkout_id to find pending transaction

        } else {
            std::cout << "STK Payment failed or cancelled: " << result_code << std::endl;
        }

        Json::Value response;
        response["ResultCode"] = 0;
        response["ResultDesc"] = "Success";
        return response;

    } catch (std::exception const & e) {
        std::cerr << "Exception in handle_stk_callback: " << e.what() << std::endl;

        Json::Value response;
        response["ResultCode"] = 1;
        response["ResultDesc"] = std::string("Error: ") + e.what();
        return response;
    }
}

Json::Value mpesa_webhook_server::handle_b2c_result(Json::Value const & request)
{
    std::cout << "B2C Result received" << std::endl;
    std::cout << request.toStyledString() << std::endl;

    try {
        Json::Value result = request["Result"];
        int result_code = result.get("ResultCode", 1).asInt();

        if (result_code == 0) {
            // B2C payment successful
            Json::Value result_parameters = result["ResultParameters"];
            Json::Value parameters = result_parameters["ResultParameter"];

            std::string transaction_receipt;
            std::string amount_str;

            for (auto const & param : parameters) {
                std::string key = param.get("Key", "").asString();

                if (key == "TransactionReceipt") {
                    transaction_receipt = param.get("Value", "").asString();
                } else if (key == "TransactionAmount") {
                    amount_str = std::to_string(param.get("Value", 0).asUInt());
                }
            }

            std::cout << "B2C Payment successful: " << transaction_receipt << std::endl;

            // TODO: Create burn operation and link to withdrawal request

        } else {
            std::cout << "B2C Payment failed: " << result_code << std::endl;
        }

        Json::Value response;
        response["ResultCode"] = 0;
        response["ResultDesc"] = "Success";
        return response;

    } catch (std::exception const & e) {
        std::cerr << "Exception in handle_b2c_result: " << e.what() << std::endl;

        Json::Value response;
        response["ResultCode"] = 1;
        response["ResultDesc"] = std::string("Error: ") + e.what();
        return response;
    }
}

} // namespace nano
