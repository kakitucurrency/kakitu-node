# Changes Made to Convert Address Prefix from "nano" to "kshs"

This document details all modifications made to the Nano node codebase to change the address prefix from "nano_" to "kshs_" while maintaining backward compatibility with existing "xrb_" addresses.

## 1. Address Encoding/Decoding Core Changes

### File: `nano/lib/numbers.cpp`
**Lines Modified: 645-650**

- **Function:** `encode_account()`
- **Change:** Modified address encoding to use "kshs_" prefix instead of "nano_"
- **Details:** Updated the prefix assignment in the account encoding function

## 2. Network Enum Definitions

### File: `nano/lib/config.hpp`
**Lines Modified: 124-135**

- **Enum:** `networks`
- **Changes:**
  - `nano_dev_network` → `kshs_dev_network`
  - `nano_beta_network` → `kshs_beta_network`
  - `nano_live_network` → `kshs_live_network`
  - `nano_test_network` → `kshs_test_network`

**Lines Modified: 335-347**
- **Function:** `set_active_network()`
- **Changes:** Updated all network enum references from `nano::networks::nano_*_network` to `nano::networks::kshs_*_network`

**Lines Modified: 365-377**
- **Functions:** Network check functions
- **Changes:** Updated return statements in:
  - `is_live_network()`
  - `is_beta_network()`
  - `is_dev_network()`
  - `is_test_network()`

## 3. Network Configuration Function

### File: `nano/lib/config.cpp`
**Lines Modified: 268-271**

- **Function:** `force_nano_dev_network()`
- **Change:** Updated network reference from `nano::networks::nano_dev_network` to `nano::networks::kshs_dev_network`

## 4. CMake Configuration

### File: `CMakeLists.txt`
**Lines Modified: 162-167**

- **Variable:** `ACTIVE_NETWORK`
- **Changes:**
  - Default value changed from `nano_live_network` to `kshs_live_network`
  - Property strings updated from `nano_dev_network nano_beta_network nano_live_network nano_test_network` to `kshs_dev_network kshs_beta_network kshs_live_network kshs_test_network`

**Lines Modified: 179, 192**
- **Conditionals:** Updated network matching conditions
  - `"${ACTIVE_NETWORK}" MATCHES "nano_beta_network"` → `"${ACTIVE_NETWORK}" MATCHES "kshs_beta_network"`
  - `"${ACTIVE_NETWORK}" MATCHES "nano_test_network"` → `"${ACTIVE_NETWORK}" MATCHES "kshs_test_network"`

## 5. Ledger Constants Structure

### File: `nano/secure/common.hpp`
**Lines Modified: 369-387**

- **Class:** `ledger_constants`
- **Field Name Changes:**
  - `nano_beta_account` → `kshs_beta_account`
  - `nano_live_account` → `kshs_live_account`
  - `nano_test_account` → `kshs_test_account`
  - `nano_dev_genesis` → `kshs_dev_genesis`
  - `nano_beta_genesis` → `kshs_beta_genesis`
  - `nano_live_genesis` → `kshs_live_genesis`
  - `nano_test_genesis` → `kshs_test_genesis`
  - `nano_dev_final_votes_canary_account` → `kshs_dev_final_votes_canary_account`
  - `nano_beta_final_votes_canary_account` → `kshs_beta_final_votes_canary_account`
  - `nano_live_final_votes_canary_account` → `kshs_live_final_votes_canary_account`
  - `nano_test_final_votes_canary_account` → `kshs_test_final_votes_canary_account`
  - `nano_dev_final_votes_canary_height` → `kshs_dev_final_votes_canary_height`
  - `nano_beta_final_votes_canary_height` → `kshs_beta_final_votes_canary_height`
  - `nano_live_final_votes_canary_height` → `kshs_live_final_votes_canary_height`
  - `nano_test_final_votes_canary_height` → `kshs_test_final_votes_canary_height`

## 6. Network Switch Statements

### File: `nano/secure/utility.cpp`
**Lines Modified: 12-29**

- **Function:** `working_path()`
- **Changes:** Updated switch case statements
  - `case nano::networks::nano_dev_network:` → `case nano::networks::kshs_dev_network:`
  - `case nano::networks::nano_beta_network:` → `case nano::networks::kshs_beta_network:`
  - `case nano::networks::nano_live_network:` → `case nano::networks::kshs_live_network:`
  - `case nano::networks::nano_test_network:` → `case nano::networks::kshs_test_network:`

### File: `nano/node/network.cpp`
**Lines Modified: 1012-1025**

- **Function:** `to_string()`
- **Changes:** Updated switch case statements
  - `case nano::networks::nano_beta_network:` → `case nano::networks::kshs_beta_network:`
  - `case nano::networks::nano_dev_network:` → `case nano::networks::kshs_dev_network:`
  - `case nano::networks::nano_live_network:` → `case nano::networks::kshs_live_network:`
  - `case nano::networks::nano_test_network:` → `case nano::networks::kshs_test_network:`

## 7. Application Entry Point

### File: `nano/nano_node/entry.cpp`
**Lines Modified: 152**

- **Function:** `main()`
- **Change:** Updated network comparison
  - `nano::networks::nano_dev_network` → `nano::networks::kshs_dev_network`

## Build Process

1. **CMake Reconfiguration:** The project was reconfigured with `cmake . -DACTIVE_NETWORK=kshs_live_network`
2. **Full Rebuild:** All source files were recompiled with the new network enum definitions
3. **Successful Build:** Both `nano_node` and `nano_rpc` executables built successfully

## Backward Compatibility

- **Maintained:** The node still accepts and processes existing "xrb_" prefixed addresses
- **New Feature:** All newly generated addresses will use the "kshs_" prefix
- **Network Protocol:** All network communication remains compatible with existing Nano network

## Testing Required

- Verify that new addresses are generated with "kshs_" prefix
- Confirm that existing "xrb_" addresses are still accepted
- Test account creation, transactions, and RPC functionality
- Validate network connectivity and block processing