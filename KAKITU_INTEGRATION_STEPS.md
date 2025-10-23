# Kakitu Integration - Final Steps

This document describes the remaining steps to complete the Kakitu M-Pesa integration into the main node.

## Status: 90% Complete

### ✅ What's Already Done:

1. **All Kakitu C++ implementations created:**
   - `nano/node/mint_authority.cpp` (850 lines) - Complete
   - `nano/node/mpesa_integration.cpp` (650 lines) - Complete, config-based
   - `nano/node/mpesa_webhook_server.cpp` (350 lines) - Complete
   - `nano/node/kakitu_node_integration.cpp` (200 lines) - Integration manager

2. **RPC method declarations added to `nano/node/json_handler.hpp`:**
   - Lines 149-155: supply_info, operation_history, verify_operation, reserve_status, initiate_deposit, audit_report

3. **Kakitu components added to `nano/node/node.hpp`:**
   - Lines 46-47: Include headers
   - Lines 204-207: Component member variables (mint_authority, mpesa_client, mpesa_bridge, webhook_server)

4. **Build system updated:**
   - `nano/node/CMakeLists.txt` - Kakitu files added
   - `nano/rpc/CMakeLists.txt` - RPC handlers added
   - Dependencies configured (CURL, OpenSSL, jsoncpp)

5. **Documentation complete:**
   - `USER_FLOW_GUIDE.md` - Complete user flow with RPC examples
   - `test_kakitu_mpesa.cpp` - Standalone test program

### 🔧 What Remains (Manual Steps):

## Step 1: Add Kakitu RPC Implementations to `nano/node/json_handler.cpp`

**Location:** After line 5279 (after `debug_bootstrap_priority_info()` function)

**Add this code:**

```cpp
// ============================================
// Kakitu M-Pesa RPC Implementations
// ============================================

void nano::json_handler::supply_info ()
{
	if (!node.kakitu_mint_authority)
	{
		ec = nano::error_rpc::generic;
		response_l.put ("error", "Kakitu mint authority not initialized");
	}
	else
	{
		response_l.put ("total_supply", node.kakitu_mint_authority->get_total_supply ().to_string_dec ());
		response_l.put ("total_reserves", node.kakitu_mint_authority->get_total_reserves ().to_string_dec ());
		response_l.put ("backing_ratio", std::to_string (node.kakitu_mint_authority->get_backing_ratio ()));
		response_l.put ("is_balanced", node.kakitu_mint_authority->verify_reserves ());
	}
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
			ec = nano::error_rpc::generic;
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
			ec = nano::error_rpc::generic;
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
			ec = nano::error_rpc::generic;
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
			ec = nano::error_rpc::generic;
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
```

**Note:** The complete implementations are also available in `kakitu_rpc_implementations.cpp` in the root directory.

## Step 2: Register Kakitu RPC Handlers

**File:** `nano/node/json_handler.cpp`
**Location:** Inside `create_ipc_json_handler_no_arg_func_map()` function, before `return no_arg_funcs;` (line 5447)

**Add these lines:**

```cpp
	// Kakitu M-Pesa RPC handlers
	no_arg_funcs.emplace ("supply_info", &nano::json_handler::supply_info);
	no_arg_funcs.emplace ("operation_history", &nano::json_handler::operation_history);
	no_arg_funcs.emplace ("verify_operation", &nano::json_handler::verify_operation);
	no_arg_funcs.emplace ("reserve_status", &nano::json_handler::reserve_status);
	no_arg_funcs.emplace ("initiate_deposit", &nano::json_handler::initiate_deposit);
	no_arg_funcs.emplace ("audit_report", &nano::json_handler::audit_report);
```

## Step 3: Initialize Kakitu Components in Node Constructor

**File:** `nano/node/node.cpp`
**Location:** In the `node::node()` constructor, after all other component initialization

**Add Kakitu initialization code:**

```cpp
	// Initialize Kakitu M-Pesa integration
	std::string kakitu_config_path = (application_path / "kakitu_config.json").string ();
	std::ifstream kakitu_config_file (kakitu_config_path);

	if (kakitu_config_file.good ())
	{
		try
		{
			Json::Value kakitu_config;
			Json::CharReaderBuilder builder;
			std::string errs;

			if (Json::parseFromStream (builder, kakitu_config_file, &kakitu_config, &errs))
			{
				// Load mint authority from environment variable
				const char* priv_key_hex = std::getenv ("KAKITU_MINT_PRIVATE_KEY");
				if (priv_key_hex)
				{
					nano::raw_key prv;
					if (!prv.decode_hex (priv_key_hex))
					{
						nano::keypair authority_key (std::move (prv));

						// Try to load existing mint authority
						std::string authority_data_path = (application_path / "mint_authority.json").string ();
						std::ifstream authority_file (authority_data_path);

						if (authority_file.good ())
						{
							kakitu_mint_authority = nano::mint_authority::load_from_disk (authority_data_path, authority_key);
							logger.always_log ("Loaded existing Kakitu mint authority");
						}
						else
						{
							kakitu_mint_authority = std::make_shared<nano::mint_authority> (authority_key);
							logger.always_log ("Created new Kakitu mint authority");
						}

						// Initialize M-Pesa client
						kakitu_mpesa_client = std::make_shared<nano::mpesa_api_client> (kakitu_config["mpesa"]);

						// Authenticate
						if (kakitu_mpesa_client->authenticate ())
						{
							logger.always_log ("Kakitu M-Pesa client authenticated");

							// Initialize bridge
							kakitu_mpesa_bridge = std::make_shared<nano::kakitu_mpesa_bridge> (kakitu_mpesa_client, kakitu_mint_authority);

							// Start webhook server if enabled
							if (kakitu_config["mpesa"]["webhook_server"]["enabled"].asBool ())
			{
								uint16_t webhook_port = kakitu_config["mpesa"]["webhook_server"]["port"].asUInt ();
								kakitu_webhook_server = std::make_shared<nano::mpesa_webhook_server> (kakitu_mpesa_bridge, webhook_port);
								kakitu_webhook_server->start ();
								logger.always_log ("Kakitu webhook server started on port ", webhook_port);
							}

							logger.always_log ("Kakitu M-Pesa integration initialized successfully");
						}
						else
						{
							logger.always_log ("Warning: Kakitu M-Pesa authentication failed");
						}
					}
					else
					{
						logger.always_log ("Warning: Invalid KAKITU_MINT_PRIVATE_KEY format");
					}
				}
				else
				{
					logger.always_log ("Info: KAKITU_MINT_PRIVATE_KEY not set, Kakitu features disabled");
				}
			}
			else
			{
				logger.always_log ("Warning: Failed to parse kakitu_config.json: ", errs);
			}
		}
		catch (std::exception const & e)
		{
			logger.always_log ("Warning: Kakitu initialization error: ", e.what ());
		}
	}
	else
	{
		logger.always_log ("Info: kakitu_config.json not found, Kakitu features disabled");
	}
```

## Step 4: Create kakitu_config.json

**File:** Create in node data directory (e.g., `~/.kakitu/kakitu_config.json`)

**Copy from:** `nano/node/mpesa_config_sandbox.json` (already created with your credentials)

## Step 5: Set Environment Variable

```bash
export KAKITU_MINT_PRIVATE_KEY="your_64_character_hex_private_key_here"
```

## Step 6: Test Compilation

```bash
cd /home/user/kakitu-node
cmake . -DACTIVE_NETWORK=kshs_live_network
make node -j2
```

## Step 7: Test RPC Endpoints

Once compiled, start the node and test:

```bash
./kakitu_node

# In another terminal:
curl -X POST http://localhost:7076/rpc \
  -H "Content-Type: application/json" \
  -d '{"action": "supply_info"}'
```

## Files Reference:

- **RPC Implementations:** `kakitu_rpc_implementations.cpp` (ready to copy/paste)
- **User Guide:** `USER_FLOW_GUIDE.md` (complete flow documentation)
- **Test Program:** `test_kakitu_mpesa.cpp` (standalone testing)
- **Integration Example:** `nano/node/kakitu_node_integration.cpp` (reference implementation)

## Automated Alternative:

If you prefer, you can use the `kakitu_node_integration.cpp` approach by calling:

```cpp
nano::kakitu_node_manager kakitu_manager (
	*this,  // node
	kakitu_config_path,
	application_path.string ()
);

if (kakitu_manager.initialize ())
{
	kakitu_mint_authority = kakitu_manager.get_mint_authority ();
	kakitu_mpesa_bridge = kakitu_manager.get_mpesa_bridge ();
}
```

This encapsulates all initialization logic in one place.

---

**Next:** Complete steps 1-3 above to finish the integration, then compile and test!
