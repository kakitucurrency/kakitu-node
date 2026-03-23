# Kakitu Production Genesis & Full Rebrand — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace all Nano-origin values (genesis key, peers, ports, magic bytes, binary names, env vars, CI) so Kakitu runs as a fully independent production network bootstrapped from a fresh genesis block.

**Architecture:** All changes are constants/configuration replacements in an existing C++ codebase. The plan is ordered so genesis-independent changes (network isolation, peering, packaging, env vars) are committed first, then a build is performed to generate the actual genesis keypair, and finally the genesis data is embedded and the final build is verified.

**Tech Stack:** C++17, CMake 3.10+, Boost, GoogleTest, systemd, GitHub Actions

**Spec:** `docs/superpowers/specs/2026-03-23-kakitu-production-genesis-design.md`

---

## File Map

| File | Change |
|------|--------|
| `nano/lib/config.hpp` | Magic bytes, live ports, remove beta port branch, remove `"beta"` from `set_active_network`, update `get_current_network_as_string` |
| `nano/lib/config.cpp` | Rename all `NANO_TEST_*` env vars to `KAKITU_TEST_*` |
| `nano/secure/utility.cpp` | Data directory names (`Nano*` → `Kakitu*`) |
| `nano/secure/common.cpp` | Genesis data, canary keys, epoch v2 signer, rename `NANO_TEST_*` env vars |
| `nano/node/nodeconfig.cpp` | Peer hostnames, env var rename, preconfigured reps, remove beta case |
| `nano/node/cli.cpp` | Config comment URL |
| `nano/nano_node/daemon.cpp` | Startup text |
| `nano/nano_node/entry.cpp` | Windows event log key (error message string) |
| `nano/lib/plat/windows/registry.cpp` | Windows event log registry key check |
| `nano/nano_node/CMakeLists.txt` | `nano_node` → `kakitu_node` |
| `nano/nano_rpc/CMakeLists.txt` | `nano_rpc` → `kakitu_rpc` |
| `CMakeLists.txt` | Project name, binary names, package metadata, service name, remove beta conditionals |
| `etc/systemd/nanocurrency.service` | Rename + update to `kakitu.service` |
| `etc/systemd/nanocurrency-test.service` | Rename + update to `kakitu-test.service` |
| `etc/systemd/nanocurrency-beta.service` | Delete |
| `nanocurrency.spec.in` | Rename + update to `kakitu.spec.in` |
| `nanocurrency-beta.spec.in` | Delete |
| `rep_weights_live.bin` | Truncate to zero bytes |
| `rep_weights_beta.bin` | Truncate to zero bytes |
| `.github/workflows/live_artifacts.yml` | Default repo → `kakitucurrency/kakitu-node` |
| `.github/workflows/test_network_artifacts.yml` | Default repo → `kakitucurrency/kakitu-node` |
| `.github/workflows/develop_branch_dockers_deploy.yml` | Repository guard + image name → kakitucurrency |
| `.github/workflows/beta_artifacts.yml` | Delete |
| `.github/workflows/beta_artifacts_latest_release_branch.yml` | Delete |
| `SECURITY.md`, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md` | Replace Nano Foundation contact/URL refs |
| `.github/ISSUE_TEMPLATE/*.yml`, `.github/ISSUE_TEMPLATE.md` | Replace nano.org refs |

---

## Task 1: Network Magic Bytes, Ports & Beta Branch Removal

**File:** `nano/lib/config.hpp`

This task isolates Kakitu from Nano at the packet level and assigns unique ports.

- [ ] **Step 1.1 — Update network magic byte enum values**

  In `nano/lib/config.hpp`, find the `networks` enum (currently around line 124):
  ```cpp
  enum class networks : uint16_t
  {
      invalid = 0x0,
      kshs_dev_network  = 0x5241, // 'R', 'A'
      kshs_beta_network = 0x5242, // 'R', 'B'
      kshs_live_network = 0x5243, // 'R', 'C'
      kshs_test_network = 0x5258, // 'R', 'X'
  };
  ```
  Replace with:
  ```cpp
  enum class networks : uint16_t
  {
      invalid = 0x0,
      kshs_dev_network  = 0x4B44, // 'K', 'D'
      kshs_beta_network = 0x4B42, // 'K', 'B'  (enum retained, no beta network runs)
      kshs_live_network = 0x4B4C, // 'K', 'L'
      kshs_test_network = 0x4B58, // 'K', 'X'
  };
  ```

- [ ] **Step 1.2 — Update live network ports**

  In the `network_constants` constructor body (inside `if (is_live_network())`), find:
  ```cpp
  default_node_port = 7075;
  default_rpc_port = 7076;
  default_ipc_port = 7077;
  default_websocket_port = 7078;
  ```
  Replace with:
  ```cpp
  default_node_port = 44075;
  default_rpc_port = 44076;
  default_ipc_port = 44077;
  default_websocket_port = 44078;
  ```

- [ ] **Step 1.3 — Remove beta port-assignment branch**

  In the same constructor, find and **delete** the entire `else if (is_beta_network())` block:
  ```cpp
  else if (is_beta_network ())
  {
      default_node_port = 54000;
      default_rpc_port = 55000;
      default_ipc_port = 56000;
      default_websocket_port = 57000;
  }
  ```

- [ ] **Step 1.4 — Remove `"beta"` from `set_active_network(std::string)`**

  Find the string overload of `set_active_network`. Delete the `"beta"` branch:
  ```cpp
  // DELETE this block:
  else if (network_a == "beta")
  {
      active_network = nano::networks::kshs_beta_network;
  }
  ```
  Update the doc comment on the function to remove `"beta"` from the list of valid values. Valid values are now: `"live"`, `"dev"`, `"test"`.

- [ ] **Step 1.5 — Update `get_current_network_as_string()`**

  Find (around line 356):
  ```cpp
  return is_live_network () ? "live" : is_beta_network () ? "beta"
  : is_test_network ()                                    ? "test"
                                                          : "dev";
  ```
  Replace with:
  ```cpp
  return is_live_network () ? "live" : is_test_network () ? "test" : "dev";
  ```

- [ ] **Step 1.6 — Verify the file compiles**

  ```bash
  cd /path/to/build && cmake /path/to/kakitu-node -DACTIVE_NETWORK=kshs_live_network
  make nano/lib/CMakeFiles/config.dir/config.cpp.o 2>&1 | head -30
  ```
  Expected: no errors (warnings OK).

- [ ] **Step 1.7 — Commit**

  ```bash
  git add nano/lib/config.hpp
  git commit -m "feat: network magic bytes KL/KD/KX, ports 44075-44078, remove beta network paths"
  ```

---

## Task 2: Data Directory Names

**File:** `nano/secure/utility.cpp`

- [ ] **Step 2.1 — Replace all data directory names**

  Find the `working_path()` function switch body. Replace the entire set of `result /=` lines:
  ```cpp
  // OLD:
  case nano::networks::kshs_dev_network:  result /= "NanoDev";  break;
  case nano::networks::kshs_beta_network: result /= "NanoBeta"; break;
  case nano::networks::kshs_live_network: result /= "Nano";     break;
  case nano::networks::kshs_test_network: result /= "NanoTest"; break;

  // NEW:
  case nano::networks::kshs_dev_network:  result /= "KakituDev";  break;
  case nano::networks::kshs_beta_network: result /= "KakituBeta"; break;
  case nano::networks::kshs_live_network: result /= "Kakitu";     break;
  case nano::networks::kshs_test_network: result /= "KakituTest"; break;
  ```

- [ ] **Step 2.2 — Commit**

  ```bash
  git add nano/secure/utility.cpp
  git commit -m "feat: rename data directories Nano* -> Kakitu*"
  ```

---

## Task 3: Environment Variable Renames in `config.cpp`

**File:** `nano/lib/config.cpp`

Replace every `NANO_TEST_*` environment variable name with `KAKITU_TEST_*`.

- [ ] **Step 3.1 — Rename test port env vars**

  Find and replace each occurrence:
  ```cpp
  // Line ~241
  auto test_env = nano::get_env_or_default ("NANO_TEST_NODE_PORT", "17075");
  // →
  auto test_env = nano::get_env_or_default ("KAKITU_TEST_NODE_PORT", "17075");

  // Line ~245
  auto test_env = nano::get_env_or_default ("NANO_TEST_RPC_PORT", "17076");
  // →
  auto test_env = nano::get_env_or_default ("KAKITU_TEST_RPC_PORT", "17076");

  // Line ~249
  auto test_env = nano::get_env_or_default ("NANO_TEST_IPC_PORT", "17077");
  // →
  auto test_env = nano::get_env_or_default ("KAKITU_TEST_IPC_PORT", "17077");

  // Line ~253
  auto test_env = nano::get_env_or_default ("NANO_TEST_WEBSOCKET_PORT", "17078");
  // →
  auto test_env = nano::get_env_or_default ("KAKITU_TEST_WEBSOCKET_PORT", "17078");
  ```

- [ ] **Step 3.2 — Rename test magic number env var and update default**

  Find (around line 262):
  ```cpp
  auto test_env = get_env_or_default ("NANO_TEST_MAGIC_NUMBER", "RX");
  ```
  Replace with:
  ```cpp
  auto test_env = get_env_or_default ("KAKITU_TEST_MAGIC_NUMBER", "KX");
  ```

- [ ] **Step 3.3 — Rename PoW threshold env vars**

  Find the `publish_test` threshold definitions (around line 47–50):
  ```cpp
  get_env_threshold_or_default ("NANO_TEST_EPOCH_1", 0xffffffc000000000),
  get_env_threshold_or_default ("NANO_TEST_EPOCH_2", 0xfffffff800000000),
  get_env_threshold_or_default ("NANO_TEST_EPOCH_2_RECV", 0xfffffe0000000000)
  ```
  Replace with:
  ```cpp
  get_env_threshold_or_default ("KAKITU_TEST_EPOCH_1", 0xffffffc000000000),
  get_env_threshold_or_default ("KAKITU_TEST_EPOCH_2", 0xfffffff800000000),
  get_env_threshold_or_default ("KAKITU_TEST_EPOCH_2_RECV", 0xfffffe0000000000)
  ```

- [ ] **Step 3.4 — Rename wallet scan reps delay env var**

  Search for `NANO_TEST_WALLET_SCAN_REPS_DELAY` in `config.cpp`. Replace with `KAKITU_TEST_WALLET_SCAN_REPS_DELAY`.

  ```bash
  grep -n "NANO_TEST_WALLET_SCAN_REPS_DELAY" nano/lib/config.cpp
  ```

- [ ] **Step 3.5 — Commit**

  ```bash
  git add nano/lib/config.cpp
  git commit -m "feat: rename NANO_TEST_* env vars to KAKITU_TEST_* in config.cpp"
  ```

---

## Task 4: Peering, Bootstrap & Remove Beta Network Constructor Case

**File:** `nano/node/nodeconfig.cpp`

- [ ] **Step 4.1 — Replace default peer hostname variables**

  Find the three static variables at the top of the anonymous namespace (around line 18–20):
  ```cpp
  std::string const default_live_peer_network = nano::get_env_or_default ("NANO_DEFAULT_PEER", "peering.nano.org");
  std::string const default_beta_peer_network = nano::get_env_or_default ("NANO_DEFAULT_PEER", "peering-beta.nano.org");
  std::string const default_test_peer_network = nano::get_env_or_default ("NANO_DEFAULT_PEER", "peering-test.nano.org");
  ```
  Replace with (delete the beta line, update the other two):
  ```cpp
  std::string const default_live_peer_network = nano::get_env_or_default ("KAKITU_DEFAULT_PEER", "peering.kakitu.org");
  std::string const default_test_peer_network = nano::get_env_or_default ("KAKITU_DEFAULT_PEER", "peering-test.kakitu.org");
  ```

- [ ] **Step 4.2 — Delete the `kshs_beta_network` constructor case**

  Find and **delete** the entire beta case (around lines 53–60):
  ```cpp
  case nano::networks::kshs_beta_network:
  {
      preconfigured_peers.emplace_back (default_beta_peer_network);
      nano::account offline_representative;
      release_assert (!offline_representative.decode_account ("kshs_1defau1t9off1ine9rep99999999999999999999999999999999wgmuzxxy"));
      preconfigured_representatives.emplace_back (offline_representative);
      break;
  }
  ```

- [ ] **Step 4.3 — Replace Nano Foundation rep keys in live network case**

  Find the `kshs_live_network` case. It currently has 8 hardcoded `preconfigured_representatives.emplace_back(...)` lines with Nano Foundation public keys. Replace the entire case body:
  ```cpp
  case nano::networks::kshs_live_network:
      preconfigured_peers.emplace_back (default_live_peer_network);
      preconfigured_representatives.emplace_back (network_params.ledger.genesis->account ());
      break;
  ```

- [ ] **Step 4.4 — Update TOML config comment**

  Search for the string `NANO_DEFAULT_PEER` in `nodeconfig.cpp` in a comment or `toml.put` call. Replace with `KAKITU_DEFAULT_PEER`.

  ```bash
  grep -n "NANO_DEFAULT_PEER" nano/node/nodeconfig.cpp
  ```

- [ ] **Step 4.5 — Verify: no `NANO_` references remain in nodeconfig.cpp**

  ```bash
  grep -n "NANO_\|nano\.org\|peering\.nano\|nano_live\|nano_beta\|nano_dev\|nano_test" nano/node/nodeconfig.cpp
  ```
  Expected: no matches.

- [ ] **Step 4.6 — Commit**

  ```bash
  git add nano/node/nodeconfig.cpp
  git commit -m "feat: peering.kakitu.org, genesis-account rep, remove beta network constructor case"
  ```

---

## Task 5: CLI Comment Update

**File:** `nano/node/cli.cpp`

- [ ] **Step 5.1 — Update the generated config file comment**

  Search for the line that begins `"# This is an example configuration file for Nano."`:
  ```bash
  grep -n "example configuration file for Nano" nano/node/cli.cpp
  ```
  Replace `Nano` with `Kakitu` and the `https://docs.nano.org/...` URL with `https://kakitu.org` in that line and any adjacent documentation URL lines in the same output block.

- [ ] **Step 5.2 — Commit**

  ```bash
  git add nano/node/cli.cpp
  git commit -m "feat: update generated config comment to reference kakitu.org"
  ```

---

## Task 6: Daemon Startup Text & Windows Event Log Key

**Files:** `nano/nano_node/daemon.cpp`, `nano/nano_node/entry.cpp`

- [ ] **Step 6.1 — Update startup message**

  In `nano/nano_node/daemon.cpp`, find (around line 102):
  ```cpp
  auto initialization_text = "Starting up Nano node...";
  ```
  Replace with:
  ```cpp
  auto initialization_text = "Starting up Kakitu node...";
  ```

- [ ] **Step 6.2 — Update Windows event log error message string**

  In `nano/nano_node/entry.cpp`, search for:
  ```bash
  grep -n "EventLog.*Nano" nano/nano_node/entry.cpp
  ```
  Replace `EventLog\\Nano\\Nano` with `EventLog\\Kakitu\\Kakitu` in the found line.

- [ ] **Step 6.3 — Update Windows event log registry check**

  In `nano/lib/plat/windows/registry.cpp`, search for:
  ```bash
  grep -n "EventLog.*Nano" nano/lib/plat/windows/registry.cpp
  ```
  Replace the registry key string `SYSTEM\\CurrentControlSet\\Services\\EventLog\\Nano\\Nano` with `SYSTEM\\CurrentControlSet\\Services\\EventLog\\Kakitu\\Kakitu`.

- [ ] **Step 6.4 — Commit**

  ```bash
  git add nano/nano_node/daemon.cpp nano/nano_node/entry.cpp nano/lib/plat/windows/registry.cpp
  git commit -m "feat: rebrand startup text and Windows event log key to Kakitu"
  ```

---

## Task 7: Binary Names — CMakeLists

**Files:** `nano/nano_node/CMakeLists.txt`, `nano/nano_rpc/CMakeLists.txt`, `CMakeLists.txt` (root)

- [ ] **Step 7.1 — Rename `nano_node` binary in `nano/nano_node/CMakeLists.txt`**

  Current content of `nano/nano_node/CMakeLists.txt` line 1:
  ```cmake
  add_executable(nano_node daemon.cpp daemon.hpp entry.cpp)
  ```
  Replace **all occurrences** of `nano_node` in this file with `kakitu_node`:
  ```cmake
  add_executable(kakitu_node daemon.cpp daemon.hpp entry.cpp)

  target_link_libraries(
    kakitu_node
    ...

  target_compile_definitions(
    kakitu_node PRIVATE ...

  set_target_properties(
    kakitu_node PROPERTIES ...

  add_custom_command(
    TARGET kakitu_node
    POST_BUILD
    COMMAND kakitu_node --generate_config node > ...
    COMMAND kakitu_node --generate_config rpc > ...

  install(TARGETS kakitu_node RUNTIME DESTINATION ...)
  ```

- [ ] **Step 7.2 — Rename `nano_rpc` binary in `nano/nano_rpc/CMakeLists.txt`**

  Replace **all occurrences** of `nano_rpc` with `kakitu_rpc` in `nano/nano_rpc/CMakeLists.txt`:
  ```cmake
  add_executable(kakitu_rpc entry.cpp)

  target_link_libraries(kakitu_rpc ...)

  target_compile_definitions(kakitu_rpc ...)

  install(TARGETS kakitu_rpc RUNTIME DESTINATION ...)
  ```

- [ ] **Step 7.3 — Update root `CMakeLists.txt` project name and package metadata**

  Find and replace:

  a) Project name (line ~17): `project(nano-node)` → `project(kakitu-node)`

  b) Package name (line ~169):
  ```cmake
  set(CPACK_PACKAGE_NAME "nano-node" CACHE STRING "" FORCE)
  ```
  → `"kakitu-node"`

  c) NSIS package name (line ~172):
  ```cmake
  set(CPACK_NSIS_PACKAGE_NAME "Nano" CACHE STRING "" FORCE)
  ```
  → `"Kakitu"`

  d) Install directory (line ~175):
  ```cmake
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "nanocurrency" CACHE STRING "" FORCE)
  ```
  → `"kakitu"`

  e) Service variable (line ~177):
  ```cmake
  set(NANO_SERVICE "nanocurrency.service")
  ```
  → `set(KAKITU_SERVICE "kakitu.service")`

  f) Vendor (line ~53):
  ```cmake
  set(CPACK_PACKAGE_VENDOR "Nano Currency")
  ```
  → `"Kakitu Currency"`

  g) NSIS info URL and contact (search for `nano.org`):
  ```cmake
  set(CPACK_NSIS_URL_INFO_ABOUT "https://nano.org")
  set(CPACK_NSIS_CONTACT "info@nano.org")
  ```
  → `"https://kakitu.org"` and `"info@kakitu.org"`

  h) Debian maintainer (search for `russel@nano.org`):
  → `"info@kakitu.org"`

  i) NSIS display name and menu links (search for `"Nano Wallet"` and `"Nano website"`):
  → `"Kakitu Wallet"` and `"Kakitu website"`

- [ ] **Step 7.4 — Remove beta network conditional blocks in root CMakeLists.txt**

  Find the block (around line 179):
  ```cmake
  if("${ACTIVE_NETWORK}" MATCHES "kshs_beta_network")
    project("nano-node-beta")
    set(CPACK_PACKAGE_NAME "nano-node-beta" ...)
    set(CPACK_NSIS_PACKAGE_NAME "Nano-Beta" ...)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "nanocurrency-beta" ...)
    set(NANO_SERVICE "nanocurrency-beta.service")
    set(NANO_PREFIX "Beta")
  ```
  Delete this entire `if` block (keep the `elseif` for test network but update it to use Kakitu names):

  ```cmake
  if("${ACTIVE_NETWORK}" MATCHES "kshs_test_network")
    project("kakitu-node-test")
    set(CPACK_PACKAGE_NAME "kakitu-node-test" CACHE STRING "" FORCE)
    set(CPACK_NSIS_PACKAGE_NAME "Kakitu-Test" CACHE STRING "" FORCE)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "kakitu-test" CACHE STRING "" FORCE)
    set(KAKITU_SERVICE "kakitu-test.service")
    set(KAKITU_PREFIX "Test")
  endif()
  ```
  **Note:** `NANO_PREFIX` is renamed to `KAKITU_PREFIX` everywhere (both in this block and wherever `${NANO_PREFIX}` is used elsewhere in `CMakeLists.txt`).

- [ ] **Step 7.5 — Update all `nano_node`/`nano_rpc` references in root CMakeLists.txt**

  ```bash
  grep -n "nano_node\|nano_rpc\|nano_wallet\|NANO_SERVICE\|nanocurrency" CMakeLists.txt
  ```
  For every occurrence:
  - `nano_node` → `kakitu_node`
  - `nano_rpc` → `kakitu_rpc`
  - `nano_wallet` → `kakitu_wallet`
  - `NANO_SERVICE` → `KAKITU_SERVICE`
  - `NANO_PREFIX` → `KAKITU_PREFIX`
  - `nanocurrency` in install/package contexts → `kakitu`

- [ ] **Step 7.5a — Verify `nano_wallet` rename is complete**

  The root `CMakeLists.txt` may reference `nano_wallet` in `install(TARGETS ...)` or `add_dependencies(...)` lines. Check:
  ```bash
  grep -n "nano_wallet" CMakeLists.txt nano/nano_node/CMakeLists.txt
  ```
  Replace every occurrence with `kakitu_wallet`. Also check whether a `nano/nano_wallet/` subdirectory exists and contains its own `CMakeLists.txt` — if so, replace `nano_wallet` → `kakitu_wallet` in that file too:
  ```bash
  ls nano/nano_wallet/ 2>/dev/null && grep -n "nano_wallet" nano/nano_wallet/CMakeLists.txt
  ```

- [ ] **Step 7.6 — Attempt build to verify CMake changes compile**

  ```bash
  cd /path/to/build
  cmake /path/to/kakitu-node -DACTIVE_NETWORK=kshs_live_network 2>&1 | tail -20
  ```
  Expected: CMake configuration succeeds, lists targets `kakitu_node`, `kakitu_rpc`.
  If errors: fix CMakeLists references before proceeding.

- [ ] **Step 7.7 — Commit**

  ```bash
  git add nano/nano_node/CMakeLists.txt nano/nano_rpc/CMakeLists.txt CMakeLists.txt
  # Include nano_wallet CMakeLists.txt if it exists:
  git add nano/nano_wallet/CMakeLists.txt 2>/dev/null || true
  git commit -m "feat: rename binaries nano_node/nano_rpc/nano_wallet -> kakitu_*, rebrand CMake metadata"
  ```

---

## Task 8: Systemd Service Files & RPM Spec

**Files:** `etc/systemd/`, `nanocurrency.spec.in`, `nanocurrency-beta.spec.in`

- [ ] **Step 8.1 — Create `etc/systemd/kakitu.service`** (rename + update live service)

  New content for `etc/systemd/kakitu.service`:
  ```ini
  [Unit]
  Description=Kakitu Live Network Daemon
  After=network.target

  [Service]
  Type=simple
  User=kakitu
  WorkingDirectory=/var/kakitu/Kakitu
  ExecStart=/usr/bin/kakitu_node --daemon --data_path=/var/kakitu/Kakitu
  Restart=on-failure

  [Install]
  WantedBy=multi-user.target
  ```

- [ ] **Step 8.2 — Create `etc/systemd/kakitu-test.service`** (rename + update test service)

  New content for `etc/systemd/kakitu-test.service`:
  ```ini
  [Unit]
  Description=Kakitu Test Network Daemon
  After=network.target

  [Service]
  Type=simple
  User=kakitu
  WorkingDirectory=/var/kakitu/KakituTest
  ExecStart=/usr/bin/kakitu_node --daemon --network=test --data_path=/var/kakitu/KakituTest
  Restart=on-failure

  [Install]
  WantedBy=multi-user.target
  ```

- [ ] **Step 8.3 — Delete old service files**

  ```bash
  git rm etc/systemd/nanocurrency.service
  git rm etc/systemd/nanocurrency-beta.service
  git rm etc/systemd/nanocurrency-test.service
  ```

- [ ] **Step 8.4 — Create `kakitu.spec.in`** (rename + update RPM spec)

  Copy `nanocurrency.spec.in` to `kakitu.spec.in`, then replace:
  - Package `Name:` → `kakitu`
  - `Summary:` → `Kakitu Currency Daemon`
  - `URL:` → `https://kakitu.org`
  - `nano_node` → `kakitu_node`
  - `nano_rpc` → `kakitu_rpc`
  - `/var/nanocurrency/Nano` → `/var/kakitu/Kakitu`
  - `nanocurrency` user/group → `kakitu`
  - `nanocurrency.service` → `kakitu.service`
  - Description text: replace "nanocurrency daemon. Nano is a digital currency..." with "Kakitu Currency daemon. Kakitu is a digital currency..."

- [ ] **Step 8.5 — Delete old spec files**

  ```bash
  git rm nanocurrency.spec.in
  git rm nanocurrency-beta.spec.in
  ```

- [ ] **Step 8.6 — Commit**

  ```bash
  git add etc/systemd/kakitu.service etc/systemd/kakitu-test.service kakitu.spec.in
  git commit -m "feat: rebrand service files and RPM spec to kakitu, delete beta artifacts"
  ```

---

## Task 9: Truncate rep_weights Files

**Files:** `rep_weights_live.bin`, `rep_weights_beta.bin`

These files contain Nano network representative weight data. Leaving them in place will corrupt the Kakitu node's initial weight calculations.

- [ ] **Step 9.1 — Truncate both files to zero bytes**

  ```bash
  truncate -s 0 rep_weights_live.bin
  truncate -s 0 rep_weights_beta.bin
  ```

  Verify:
  ```bash
  ls -la rep_weights_live.bin rep_weights_beta.bin
  ```
  Expected: both files show `0` bytes.

- [ ] **Step 9.2 — Commit**

  ```bash
  git add rep_weights_live.bin rep_weights_beta.bin
  git commit -m "feat: clear Nano rep_weights data — node will build from genesis ledger"
  ```

---

## Task 10: CI/CD Workflows & Community Files

**Files:** `.github/workflows/`, `SECURITY.md`, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `.github/ISSUE_TEMPLATE/`

- [ ] **Step 10.1 — Delete beta workflow files**

  ```bash
  git rm .github/workflows/beta_artifacts.yml
  git rm .github/workflows/beta_artifacts_latest_release_branch.yml
  ```

- [ ] **Step 10.2 — Update `live_artifacts.yml`**

  Find the `default:` value under the `repo:` input (around line 7):
  ```yaml
  default: "nanocurrency/nano-node"
  ```
  Replace with:
  ```yaml
  default: "kakitucurrency/kakitu-node"
  ```
  Also update the Docker build/push step that references `nanocurrency/nano` image name. Find lines like:
  ```yaml
  run: CI_TAG=${TAG} ci/actions/linux/docker-build.sh
  # and
  if: ${{ github.repository == 'nanocurrency/nano-node' }}
  ```
  Update the repository guard condition to `'kakitucurrency/kakitu-node'`.

- [ ] **Step 10.3 — Update `test_network_artifacts.yml`**

  Same as above — replace `nanocurrency/nano-node` default with `kakitucurrency/kakitu-node`.

- [ ] **Step 10.3a — Update `develop_branch_dockers_deploy.yml`**

  ```bash
  grep -n "nanocurrency\|nano-node\|nano\.org" .github/workflows/develop_branch_dockers_deploy.yml
  ```
  This file builds and pushes Docker images on every push to the develop branch. Update:
  - Any `if: ${{ github.repository == 'nanocurrency/nano-node' }}` guard → `'kakitucurrency/kakitu-node'`
  - Any `nanocurrency/nano` Docker image name → `kakitucurrency/kakitu`
  - Any `nano-node` references in image tags or registry paths → `kakitu-node`

- [ ] **Step 10.4 — Update `SECURITY.md`**

  Replace all `nano.org` contact email/URL references with `kakitu.org` equivalents. Replace Nano Foundation security team members with Kakitu team. Remove references to the external Nano security audit PDF.

- [ ] **Step 10.5 — Update `CONTRIBUTING.md`**

  Replace `https://docs.nano.org/core-development/...` links with `https://kakitu.org` or remove them if no equivalent page exists yet.

- [ ] **Step 10.6 — Update `CODE_OF_CONDUCT.md`**

  Replace `moderation@nano.org` with `info@kakitu.org`.

- [ ] **Step 10.7 — Update issue templates**

  ```bash
  grep -rl "nano.org\|nanocurrency\|nano-node" .github/
  ```
  For each file found, replace `nano.org` references with `kakitu.org` and `nanocurrency/nano-node` with `kakitucurrency/kakitu-node`.

- [ ] **Step 10.8 — Verify no Nano.org references remain in community files**

  ```bash
  grep -r "nano\.org\|nanocurrency/nano-node\|forum\.nano\|docs\.nano" \
    SECURITY.md CONTRIBUTING.md CODE_OF_CONDUCT.md .github/
  ```
  Expected: no matches.

- [ ] **Step 10.9 — Commit**

  ```bash
  git add .github/ SECURITY.md CONTRIBUTING.md CODE_OF_CONDUCT.md
  git commit -m "feat: update CI workflows and community files to kakitucurrency/kakitu-node"
  ```

---

## Task 11: Build the Node — Generate Keypairs (Operational)

**This task produces values needed for Task 12. No code changes are committed here.**

Before this step, the codebase compiles but still has Nano genesis data. That is intentional — you need a working binary to generate the new keys.

- [ ] **Step 11.1 — Build `kakitu_node`**

  ```bash
  cd /path/to/build
  cmake /path/to/kakitu-node \
    -DACTIVE_NETWORK=kshs_live_network \
    -DCMAKE_BUILD_TYPE=Release
  make kakitu_node -j$(nproc)
  ```
  Expected: `kakitu_node` binary in the build directory.

- [ ] **Step 11.2 — Generate the live genesis keypair**

  ```bash
  ./kakitu_node --key_create
  ```
  Output will be three lines:
  ```
  Private: <64 hex chars>
  Public: <64 hex chars>
  Account: kshs_1...
  ```
  **CRITICAL SECURITY:** Write the private key down and store it offline (cold storage). This key controls 100% of the Kakitu supply at genesis. Never put it in a file that is committed to git, uploaded to any server, or stored in plaintext on an internet-connected machine.

  Record all three values — you will need them in Task 12.

- [ ] **Step 11.3 — Generate live canary keypair**

  ```bash
  ./kakitu_node --key_create
  ```
  Record as `CANARY_LIVE_PRIVATE`, `CANARY_LIVE_PUBLIC`, `CANARY_LIVE_ACCOUNT`.

- [ ] **Step 11.4 — Generate beta canary keypair**

  ```bash
  ./kakitu_node --key_create
  ```
  Record as `CANARY_BETA_PRIVATE`, `CANARY_BETA_PUBLIC`, `CANARY_BETA_ACCOUNT`.
  (Even without a running beta network, the code path still references this value.)

- [ ] **Step 11.5 — Generate epoch v2 signer keypair**

  ```bash
  ./kakitu_node --key_create
  ```
  Record as `EPOCH_V2_PRIVATE`, `EPOCH_V2_PUBLIC`, `EPOCH_V2_ACCOUNT`.
  **CRITICAL:** Store private key offline. This key is required to perform future epoch v2 upgrades on the live network.

- [ ] **Step 11.6 — Generate the live genesis open block**

  Start a dev node (uses low PoW — fine for generating the genesis block structure, but the final work must be computed at live difficulty):
  ```bash
  # Start dev node in background with RPC enabled
  ./kakitu_node --network=dev --daemon \
    --config rpc.enable=true \
    --config rpc.port=45000 &

  # Wait for node to start
  sleep 3

  # Confirm RPC is responding
  curl -s -d '{"action":"version"}' http://localhost:45000
  # Expected: JSON with "node_vendor" field — if connection refused, wait 3 more seconds

  # Generate PoW for the genesis open block
  # Root for an open block = the account's public key
  curl -s -d '{"action":"work_generate","hash":"<LIVE_GENESIS_PUBLIC_KEY>","difficulty":"ffffffc000000000"}' \
    http://localhost:45000 | python3 -m json.tool
  ```
  Record the `work` value from the response.

  Then construct and sign the genesis block:
  ```bash
  curl -s -d '{
    "action": "block_create",
    "type": "open",
    "key": "<LIVE_GENESIS_PRIVATE_KEY>",
    "account": "<LIVE_GENESIS_ACCOUNT>",
    "representative": "<LIVE_GENESIS_ACCOUNT>",
    "source": "<LIVE_GENESIS_PUBLIC_KEY>",
    "work": "<WORK_FROM_ABOVE>"
  }' http://localhost:45000 | python3 -m json.tool
  ```
  Record the full JSON block from the `"block"` field of the response. You need: `type`, `source`, `representative`, `account`, `work`, `signature`.

  Stop the dev node:
  ```bash
  pkill kakitu_node
  ```

  **Verify the block is valid** by asking the dev node to process it:
  ```bash
  # Restart dev node with RPC enabled
  ./kakitu_node --network=dev --daemon \
    --config rpc.enable=true \
    --config rpc.port=45000 &
  sleep 3
  curl -s -d '{"action":"process","block":<BLOCK_JSON>}' http://localhost:45000
  # Expected: {"hash": "<block_hash>"}  (no "error" field)
  pkill kakitu_node
  ```

---

## Task 12: Embed Genesis Data, Canary Keys & Epoch Signer

**File:** `nano/secure/common.cpp`

This task uses the keypairs generated in Task 11. Do not proceed without them.

- [ ] **Step 12.1 — Replace `live_public_key_data`**

  Find (around line 31):
  ```cpp
  char const * live_public_key_data = "E89208DD038FBB269987689621D52292AE9C35941A7484756ECCED92A65093BA"; // xrb_3t6k35...
  ```
  Replace with:
  ```cpp
  char const * live_public_key_data = "<LIVE_GENESIS_PUBLIC_KEY>"; // <LIVE_GENESIS_ACCOUNT>
  ```

- [ ] **Step 12.2 — Replace `live_genesis_data`**

  Find the `live_genesis_data` block (lines 51–58). Replace the entire JSON:
  ```cpp
  char const * live_genesis_data = R"%%%({
      "type": "open",
      "source": "<LIVE_GENESIS_PUBLIC_KEY>",
      "representative": "<LIVE_GENESIS_ACCOUNT>",
      "account": "<LIVE_GENESIS_ACCOUNT>",
      "work": "<WORK_VALUE>",
      "signature": "<SIGNATURE_VALUE>"
      })%%%";
  ```

- [ ] **Step 12.3 — Update `dev_genesis_data` address prefix**

  Find `dev_genesis_data` (lines 33–40). The representative and account fields still use `xrb_`:
  ```cpp
  "representative": "xrb_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
  "account": "xrb_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
  ```
  Replace `xrb_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo` with the same address re-encoded as `kshs_`. Get the kshs_ version:
  ```bash
  # 34F0A37AAD... is the dev PRIVATE key; B0311EA5... is the derived public key
  # Use key_expand (takes private key) to get the kshs_ address:
  ./kakitu_node --network=dev --daemon \
    --config rpc.enable=true \
    --config rpc.port=45000 &
  sleep 3
  curl -s -d '{"action":"key_expand","key":"34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4"}' \
    http://localhost:45000
  pkill kakitu_node
  ```
  The `account` field in the response is the `kshs_` address. Update both `representative` and `account` in `dev_genesis_data`.

  Also update the inline comment on `dev_public_key_data` (line 29) from `// xrb_3e3j5...` to `// kshs_3e3j5...` (using the address from the above step).

- [ ] **Step 12.4 — Remove `beta_public_key_data` and `beta_genesis_data`**

  Delete these two variable declarations entirely (lines 30 and 42–49).

- [ ] **Step 12.5 — Rename env vars for test genesis**

  Find (around line 32):
  ```cpp
  std::string const test_public_key_data = nano::get_env_or_default ("NANO_TEST_GENESIS_PUB", ...);
  ```
  Replace `NANO_TEST_GENESIS_PUB` → `KAKITU_TEST_GENESIS_PUB`.

  Find (around line 60):
  ```cpp
  std::string const test_genesis_data = nano::get_env_or_default ("NANO_TEST_GENESIS_BLOCK", ...);
  ```
  Replace `NANO_TEST_GENESIS_BLOCK` → `KAKITU_TEST_GENESIS_BLOCK`.

- [ ] **Step 12.6 — Replace `live_canary_public_key_data`**

  Find (around line 78):
  ```cpp
  char const * live_canary_public_key_data = "7CBAF192..."; // kshs_1z7ty8...
  ```
  Replace with:
  ```cpp
  char const * live_canary_public_key_data = "<CANARY_LIVE_PUBLIC>"; // <CANARY_LIVE_ACCOUNT>
  ```

- [ ] **Step 12.7 — Replace `beta_canary_public_key_data`**

  Find (around line 77):
  ```cpp
  char const * beta_canary_public_key_data = "868C6A9F..."; // kshs_33nef...
  ```
  Replace with:
  ```cpp
  char const * beta_canary_public_key_data = "<CANARY_BETA_PUBLIC>"; // <CANARY_BETA_ACCOUNT>
  ```

- [ ] **Step 12.8 — Rename `NANO_TEST_CANARY_PUB` env var**

  Find (around line 79):
  ```cpp
  std::string const test_canary_public_key_data = nano::get_env_or_default ("NANO_TEST_CANARY_PUB", ...);
  ```
  Replace `NANO_TEST_CANARY_PUB` → `KAKITU_TEST_CANARY_PUB`.

- [ ] **Step 12.9 — Replace epoch v2 signer account**

  Find the `kshs_live_epoch_v2_signer.decode_account` call. Replace the address string:
  ```cpp
  // OLD (Nano Foundation's key):
  auto error (kshs_live_epoch_v2_signer.decode_account ("kshs_3qb6o6i1tkzr6jwr5s7eehfxwg9x6eemitdinbpi7u8bjjwsgqfj4wzser3x"));
  // NEW:
  auto error (kshs_live_epoch_v2_signer.decode_account ("<EPOCH_V2_ACCOUNT>"));
  ```

- [ ] **Step 12.10 — Fix ledger_constants initialiser list for removed beta fields**

  After deleting `beta_public_key_data` and `beta_genesis_data`, the `ledger_constants` initialiser list (around line 103–115) references `beta_public_key_data` in:
  ```cpp
  kshs_beta_account (beta_public_key_data),
  ...
  kshs_beta_genesis (parse_block_from_genesis_data (beta_genesis_data)),
  ```
  You must provide replacement values. Options:
  - Use the live public key for `kshs_beta_account` (no beta network runs, so this is never used)
  - Use an empty/zero string for `beta_genesis_data`

  Simplest approach — add a local placeholder:
  ```cpp
  // At the top of the anonymous namespace, after the other declarations:
  char const * beta_public_key_data = live_public_key_data; // no beta network
  char const * beta_genesis_data = live_genesis_data;       // no beta network
  ```
  This keeps the code compiling without structural changes to `ledger_constants`.

- [ ] **Step 12.11 — Verify: no Nano genesis keys remain**

  ```bash
  grep -n "E89208DD\|xrb_3t6k35\|7CBAF192\|868C6A9F\|3qb6o6i1" nano/secure/common.cpp
  ```
  Expected: no matches.

- [ ] **Step 12.12 — Commit**

  ```bash
  git add nano/secure/common.cpp
  git commit -m "feat: embed Kakitu live genesis block, canary keys, epoch v2 signer"
  ```
  **Note:** Do NOT commit the private key. It must never appear in any committed file.

---

## Task 13: Final Build & Verification

- [ ] **Step 13.1 — Full clean rebuild**

  ```bash
  cd /path/to/build
  cmake /path/to/kakitu-node \
    -DACTIVE_NETWORK=kshs_live_network \
    -DCMAKE_BUILD_TYPE=Release
  make kakitu_node kakitu_rpc -j$(nproc) 2>&1 | tail -20
  ```
  Expected: both binaries build without errors.

- [ ] **Step 13.2 — Verify genesis block loads correctly**

  ```bash
  ./kakitu_node --network=dev --daemon \
    --config rpc.enable=true \
    --config rpc.port=45000 &
  sleep 3
  curl -s -d '{"action":"block_info","hash":"<LIVE_GENESIS_BLOCK_HASH>"}' http://localhost:45000
  pkill kakitu_node
  ```
  Expected: returns the genesis block data with no `"error"` field.

- [ ] **Step 13.3 — Verify live network port**

  ```bash
  ./kakitu_node --network=live --daemon &
  sleep 3
  ss -tlnp | grep kakitu_node
  # or:
  lsof -i :44075
  ```
  Expected: `kakitu_node` listening on port `44075`.
  ```bash
  pkill kakitu_node
  ```

- [ ] **Step 13.4 — Verify data directory**

  ```bash
  ls ~/.local/share/   # Linux
  # or:
  ls ~/Library/Application\ Support/   # macOS
  ```
  Expected: a `Kakitu` directory (for live network) or `KakituDev` (for dev), NOT `Nano` or `NanoDev`.

- [ ] **Step 13.5 — Run unit tests (if build configured with NANO_TEST=ON)**

  ```bash
  cmake /path/to/kakitu-node -DNANO_TEST=ON -DACTIVE_NETWORK=kshs_dev_network
  make core_test -j$(nproc)
  ./core_test --gtest_filter="uint256_union*:block*:config*" 2>&1 | tail -30
  ```
  Expected: all selected tests PASS.

- [ ] **Step 13.6 — Final commit with version tag**

  ```bash
  git add -A
  git commit -m "chore: final verification — Kakitu node ready for genesis launch"
  git tag -a v1.0.0-kakitu -m "Kakitu network genesis release"
  ```

---

## Critical Operational Steps (After Code Changes, Before Launch)

These are NOT code changes — they are server/infrastructure steps:

1. **Set up the genesis node server** with ports `44075/44076/44077/44078` open in firewall
2. **Create DNS A record:** `peering.kakitu.org` → genesis node IP
3. **Copy the private key** to the genesis node (air-gapped transfer, then delete from all machines)
4. **Start the genesis node** — it will bootstrap from its own ledger (no peers needed for the first node)
5. **Verify block 1** exists in the ledger: `curl -d '{"action":"block_count"}' http://localhost:44076`
6. **Distribute the genesis account's funds** to wallets/exchanges as needed via RPC
7. **Only then invite other nodes** to `peering.kakitu.org`

---

## Verification Checklist

Run these commands on the production genesis node before announcing network launch:

```bash
# 1. Verify binary name
ls -la /usr/bin/kakitu_node

# 2. Verify live port
ss -tlnp | grep 44075

# 3. Verify genesis block
curl -s -d '{"action":"block_count"}' http://localhost:44076
# Expected: {"count": "1", "unchecked": "0", "cemented": "1"}

# 4. Verify genesis account
curl -s -d '{"action":"account_info","account":"<LIVE_GENESIS_ACCOUNT>"}' http://localhost:44076
# Expected: balance = max uint128

# 5. Verify no Nano peers in peer list
curl -s -d '{"action":"peers"}' http://localhost:44076
# Expected: empty or only your own nodes

# 6. Verify data directory
ls /var/kakitu/Kakitu/
# Expected: ledger, wallets.ldb, config-node.toml, etc.
```
