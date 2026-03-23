# Kakitu Node — Production Genesis & Full Rebrand Design

**Date:** 2026-03-23
**Status:** Approved
**Scope:** Production-readiness audit — genesis block generation, network isolation, peering infrastructure, binary rebrand, env vars, CI/CD

---

## Background

Kakitu is a fork of the Nano node (v25.0) customised with the `kshs_` address prefix. An initial partial rebrand was applied (network enum names, address encoding, some test files) but the codebase retains numerous Nano-specific values that make it unsafe and non-functional for production:

- The live genesis key/block is still the original Nano Foundation genesis (anyone with that private key controls all supply)
- Peering servers point to `peering.nano.org`
- Preconfigured reps are Nano Foundation validators
- Live port conflicts with Nano (`7075`)
- Data directories, binary names, service files, env vars, and CI all branded as Nano

This spec covers the complete set of changes required to make Kakitu safe and independently operable as a production network.

---

## Decisions

| Decision | Choice |
|----------|--------|
| Genesis keypair | Generate fresh (private key held by Kakitu team) |
| Peering domain | `peering.kakitu.org` |
| Live ports | `44075 / 44076 / 44077 / 44078` |
| Beta network | None — live + dev only |
| Initial representatives | Genesis account only |
| Binary names | `kakitu_node`, `kakitu_rpc`, `kakitu_wallet` |
| Data directories | `~/Kakitu/`, `~/KakituDev/`, `~/KakituTest/` |
| Network magic bytes | `KL` (live), `KD` (dev), `KX` (test) — beta enum value retained in code as `KB` but no beta network runs |

---

## Section 1 — Genesis Block & Network Identity

### 1.1 Generate live genesis keypair

Run on the genesis node (keep the private key secret and offline after use):
```bash
./kakitu_node --key_create
```
Output: `Private key`, `Public key`, `Account` (`kshs_1...`).

### 1.2 Generate the live genesis open block

The genesis block is a legacy `open` block with:
- `source` = genesis public key
- `account` = genesis `kshs_1...` address
- `representative` = same address (genesis is its own rep at launch)
- `work` = valid PoW with root = genesis public key, threshold = `publish_full.epoch_1` (`0xffffffc000000000`)
- `signature` = Ed25519 signature of block hash using genesis private key

Generation process:
1. Start a dev node (`./kakitu_node --network=dev --daemon`)
2. Use RPC `key_expand` to import the new private key
3. Use RPC `work_generate` with root = genesis public key
4. Construct and sign the open block via RPC `block_create`
5. Record the resulting JSON (type, source, representative, account, work, signature)

### 1.3 Embed genesis data in `nano/secure/common.cpp`

Current state: `live_genesis_data` uses Nano Foundation's public key `E89208DD...` and `xrb_3t6k35...` addresses. `dev_genesis_data` uses `xrb_3e3j5...` addresses.

Replace/update entries:

| Field | Action |
|-------|--------|
| `live_public_key_data` | Replace with new genesis public key |
| `live_genesis_data` | Replace entirely — new block JSON, all `kshs_` addresses, new work + signature |
| `dev_public_key_data` / `dev_private_key_data` | Keep Nano dev key values (publicly known, fine for local dev); update the inline comment from `xrb_` to the equivalent `kshs_` address |
| `dev_genesis_data` | Update `xrb_` address strings to `kshs_` prefix only (key/work/signature unchanged) |
| `beta_public_key_data` / `beta_genesis_data` | Remove (no beta network) |
| `test_public_key_data` / `test_genesis_data` | Keep env-var driven — block content unchanged, but env var name changes per Section 5 (`NANO_TEST_GENESIS_PUB` → `KAKITU_TEST_GENESIS_PUB`, `NANO_TEST_GENESIS_BLOCK` → `KAKITU_TEST_GENESIS_BLOCK`) |

### 1.4 Generate fresh canary keypairs

Two new keypairs (you hold the private keys):
- `live_canary_public_key_data` — replaces `7CBAF192...`
- `beta_canary_public_key_data` — replaces `868C6A9F...` (retained in code as the `kshs_beta_final_votes_canary_account` field still exists in `ledger_constants`)

### 1.5 Generate fresh epoch v2 signer keypair

In `nano/secure/common.cpp`, replace the account string `"kshs_3qb6o6i1tkzr6jwr5s7eehfxwg9x6eemitdinbpi7u8bjjwsgqfj4wzser3x"` (the `kshs_live_epoch_v2_signer` decode call) with a new `kshs_` address derived from a keypair you control. This key authorises epoch v2 upgrades on the live network. Private key must be kept secure offline.

### 1.6 Clear rep_weights files

`rep_weights_live.bin` (currently 6,544 bytes of Nano network data) and `rep_weights_beta.bin` (currently 736 bytes) must be replaced with empty zero-byte files. The node builds its own weight table from the ledger at startup. These are pending changes — not yet applied.

---

## Section 2 — Network Isolation

### 2.1 Network magic bytes (`nano/lib/config.hpp`)

Current values (Nano-origin, unchanged):
```cpp
kshs_dev_network  = 0x5241,  // 'R', 'A'
kshs_beta_network = 0x5242,  // 'R', 'B'
kshs_live_network = 0x5243,  // 'R', 'C'
kshs_test_network = 0x5258,  // 'R', 'X'
```

Replace with:
```cpp
kshs_dev_network  = 0x4B44,  // 'K', 'D'
kshs_beta_network = 0x4B42,  // 'K', 'B'  (enum value retained even though no beta network runs)
kshs_live_network = 0x4B4C,  // 'K', 'L'
kshs_test_network = 0x4B58,  // 'K', 'X'
```

Ensures all Kakitu packets are immediately rejected by Nano nodes and vice versa.

### 2.2 Live network ports (`nano/lib/config.hpp`)

Current live ports (conflict with Nano):
```cpp
default_node_port = 7075;  default_rpc_port = 7076;
default_ipc_port  = 7077;  default_websocket_port = 7078;
```

Replace with:
```cpp
default_node_port      = 44075;
default_rpc_port       = 44076;
default_ipc_port       = 44077;
default_websocket_port = 44078;
```

Dev network retains `44000/45000/46000/47000` (local-only, no conflict risk).
Test network ports are env-var driven (`KAKITU_TEST_NODE_PORT` etc., renamed in Section 5) — no hardcoded defaults to change.

The `is_beta_network()` branch inside the `network_constants` constructor (`nano/lib/config.hpp`) that assigns beta ports `54000/55000/56000/57000` is **removed**. With no beta network this is dead code and a risk if accidentally triggered.

The `set_active_network(std::string)` function in `nano/lib/config.hpp` currently accepts `"beta"` as a valid string (sets `kshs_beta_network`). The `"beta"` branch is **removed** — passing `--network=beta` at the CLI will now return an error, which is the correct behaviour. The doc comment on that function is updated to remove `"beta"` from the list of valid values.

The `get_current_network_as_string()` inline in `config.hpp` also has an `is_beta_network() ? "beta"` branch — this is updated to remove the beta case (it will fall through to the `else` / unknown path, which is acceptable since beta is no longer a valid active network).

### 2.3 Data directory paths (`nano/secure/utility.cpp`)

Current `working_path()` function has cases for `NanoDev`, `NanoBeta`, `Nano`, `NanoTest`. Replace the entire switch body:

```cpp
case nano::networks::kshs_dev_network:
    result /= "KakituDev";
    break;
case nano::networks::kshs_beta_network:
    result /= "KakituBeta";  // retained for completeness even with no beta network running
    break;
case nano::networks::kshs_live_network:
    result /= "Kakitu";
    break;
case nano::networks::kshs_test_network:
    result /= "KakituTest";
    break;
```

### 2.4 Test magic number default (`nano/lib/config.cpp`)

```cpp
// Current:
auto test_env = get_env_or_default ("NANO_TEST_MAGIC_NUMBER", "RX");
// Replace with:
auto test_env = get_env_or_default ("KAKITU_TEST_MAGIC_NUMBER", "KX");
```

---

## Section 3 — Peering & Bootstrap Infrastructure

### 3.1 Default peer hostnames (`nano/node/nodeconfig.cpp`)

Current three variables (all pointing to Nano):
```cpp
std::string const default_live_peer_network  = nano::get_env_or_default ("NANO_DEFAULT_PEER", "peering.nano.org");
std::string const default_beta_peer_network  = nano::get_env_or_default ("NANO_DEFAULT_PEER", "peering-beta.nano.org");
std::string const default_test_peer_network  = nano::get_env_or_default ("NANO_DEFAULT_PEER", "peering-test.nano.org");
```

Replace with two variables (beta removed):
```cpp
std::string const default_live_peer_network =
    nano::get_env_or_default ("KAKITU_DEFAULT_PEER", "peering.kakitu.org");
std::string const default_test_peer_network =
    nano::get_env_or_default ("KAKITU_DEFAULT_PEER", "peering-test.kakitu.org");
```

### 3.2 DNS setup (operational — not a code change)

Create DNS A record before launch:
```
peering.kakitu.org  →  <genesis node public IP>
```

### 3.3 Preconfigured representatives and beta case removal (`nano/node/nodeconfig.cpp`)

The `kshs_beta_network` constructor case (currently lines 53–60) is **deleted entirely** along with the `default_beta_peer_network` variable removal.

Live network case replaces the 8 Nano Foundation rep keys:
```cpp
case nano::networks::kshs_live_network:
    preconfigured_peers.emplace_back (default_live_peer_network);
    preconfigured_representatives.emplace_back (network_params.ledger.genesis->account ());
    break;
```

### 3.4 rep_weights files

`rep_weights_live.bin` and `rep_weights_beta.bin` truncated to zero bytes (see Section 1.6).

---

## Section 4 — Binary, Service & Packaging Rebrand

### 4.1 Binary names

Files to update: `CMakeLists.txt` (root), `nano/nano_node/CMakeLists.txt`, `nano/nano_rpc/CMakeLists.txt`

Note: There is no `nano/nano_wallet/CMakeLists.txt` — the wallet binary (`nano_wallet`) is defined entirely inside the root `CMakeLists.txt`.

| Old | New |
|-----|-----|
| `nano_node` | `kakitu_node` |
| `nano_rpc` | `kakitu_rpc` |
| `nano_wallet` | `kakitu_wallet` |

All `add_executable()`, `install(TARGETS ...)`, `target_link_libraries()`, and any string references to these binary names updated throughout.

Note: `nano/rpc/CMakeLists.txt` defines the `nano_rpc` **library** (not binary) — its target name is left as-is since it is an internal library name, not a user-facing binary.

### 4.2 CMake package metadata (`CMakeLists.txt`)

```cmake
project(kakitu-node)
set(CPACK_PACKAGE_NAME              "kakitu-node"  CACHE STRING "" FORCE)
set(CPACK_PACKAGE_VENDOR            "Kakitu Currency")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "kakitu"       CACHE STRING "" FORCE)
set(CPACK_NSIS_PACKAGE_NAME         "Kakitu"       CACHE STRING "" FORCE)
set(CPACK_NSIS_URL_INFO_ABOUT       "https://kakitu.org")
set(CPACK_NSIS_CONTACT              "info@kakitu.org")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "info@kakitu.org")
set(KAKITU_SERVICE                  "kakitu.service")
```

The beta-network conditional blocks inside `CMakeLists.txt` that set `nano-node-beta` / `nanocurrency-beta` package names are removed.

### 4.3 Systemd service files

Three files exist in `etc/systemd/`:

- `nanocurrency.service` → **rename** to `kakitu.service`: update binary (`kakitu_node`), description (`Kakitu Currency Daemon`), user/group (`kakitu`), home (`/var/kakitu/`)
- `nanocurrency-test.service` → **rename** to `kakitu-test.service`: update binary and paths equivalently
- `nanocurrency-beta.service` → **delete** (no beta network)

### 4.4 RPM spec (`nanocurrency.spec.in` → `kakitu.spec.in`)

- Package name: `kakitu`
- Binaries: `kakitu_node`, `kakitu_rpc`
- Home dir: `/var/kakitu/Kakitu`
- User/group: `kakitu`
- URL: `https://kakitu.org`

`nanocurrency-beta.spec.in` — deleted (no beta network).

### 4.5 Daemon startup text (`nano/nano_node/daemon.cpp`)

```cpp
auto initialization_text = "Starting up Kakitu node...";
```

### 4.6 Windows event log key (`nano/nano_node/entry.cpp`)

```
EventLog\\Kakitu\\Kakitu
```

---

## Section 5 — Environment Variables

All `NANO_*` env vars renamed to `KAKITU_*` in source:

| Old | New | File |
|-----|-----|------|
| `NANO_DEFAULT_PEER` | `KAKITU_DEFAULT_PEER` | `nodeconfig.cpp` |
| `NANO_TEST_GENESIS_PUB` | `KAKITU_TEST_GENESIS_PUB` | `common.cpp` |
| `NANO_TEST_GENESIS_BLOCK` | `KAKITU_TEST_GENESIS_BLOCK` | `common.cpp` |
| `NANO_TEST_CANARY_PUB` | `KAKITU_TEST_CANARY_PUB` | `common.cpp` |
| `NANO_TEST_MAGIC_NUMBER` | `KAKITU_TEST_MAGIC_NUMBER` | `config.cpp` |
| `NANO_TEST_EPOCH_1` | `KAKITU_TEST_EPOCH_1` | `config.cpp` |
| `NANO_TEST_EPOCH_2` | `KAKITU_TEST_EPOCH_2` | `config.cpp` |
| `NANO_TEST_EPOCH_2_RECV` | `KAKITU_TEST_EPOCH_2_RECV` | `config.cpp` |
| `NANO_TEST_NODE_PORT` | `KAKITU_TEST_NODE_PORT` | `config.cpp` |
| `NANO_TEST_RPC_PORT` | `KAKITU_TEST_RPC_PORT` | `config.cpp` |
| `NANO_TEST_IPC_PORT` | `KAKITU_TEST_IPC_PORT` | `config.cpp` |
| `NANO_TEST_WEBSOCKET_PORT` | `KAKITU_TEST_WEBSOCKET_PORT` | `config.cpp` |
| `NANO_TEST_WALLET_SCAN_REPS_DELAY` | `KAKITU_TEST_WALLET_SCAN_REPS_DELAY` | `config.cpp` |

TOML config comment in `nodeconfig.cpp` updated to reference `KAKITU_DEFAULT_PEER`.
CLI config generation comment in `cli.cpp` updated to reference `https://kakitu.org`.

---

## Section 6 — CI/CD Workflows

### 6.1 Workflow repo references (`.github/workflows/`)

| File | Change |
|------|--------|
| `live_artifacts.yml` | Default repo → `kakitucurrency/kakitu-node` |
| `test_network_artifacts.yml` | Default repo → `kakitucurrency/kakitu-node` |
| `beta_artifacts.yml` | **Delete** (no beta network) |
| `beta_artifacts_latest_release_branch.yml` | **Delete** (no beta network) |

### 6.2 Docker output image name

CI steps that publish the Docker image updated from `nanocurrency/nano` to `kakitucurrency/kakitu`.
Build environment images (`nanocurrency/nano-env:*`) kept as-is — they are public toolchain images, not network-specific.

### 6.3 Community & documentation files

Update all `nano.org`, `forum.nano.org`, `docs.nano.org`, Nano Foundation contact details in:
- `SECURITY.md`
- `CONTRIBUTING.md`
- `CODE_OF_CONDUCT.md`
- `.github/ISSUE_TEMPLATE/*.yml`
- `.github/ISSUE_TEMPLATE.md`

Replace with `kakitu.org` equivalents.

---

## Files Changed Summary

### Modified
- `nano/secure/common.cpp` — genesis keys/blocks, canary keys, epoch v2 signer, env var names, xrb_ → kshs_ in dev_genesis_data
- `nano/secure/utility.cpp` — data directory names (all four cases including beta)
- `nano/lib/config.hpp` — network magic bytes, live ports
- `nano/lib/config.cpp` — env var names (test ports, magic number, PoW thresholds, wallet scan delay)
- `nano/node/nodeconfig.cpp` — peer hostnames, env var name, preconfigured reps, beta case removed
- `nano/node/cli.cpp` — config comment URL
- `nano/nano_node/daemon.cpp` — startup text
- `nano/nano_node/entry.cpp` — Windows event log key
- `CMakeLists.txt` — project name, binary names, package metadata, service name, beta conditionals removed
- `nano/nano_node/CMakeLists.txt` — `kakitu_node` binary target name
- `nano/nano_rpc/CMakeLists.txt` — `kakitu_rpc` binary target name
- `.github/workflows/live_artifacts.yml` — repo default
- `.github/workflows/test_network_artifacts.yml` — repo default
- `SECURITY.md`, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md` — branding
- `.github/ISSUE_TEMPLATE/*.yml` — branding

### Deleted
- `.github/workflows/beta_artifacts.yml`
- `.github/workflows/beta_artifacts_latest_release_branch.yml`
- `nanocurrency-beta.spec.in`

### Truncated to zero bytes (pending)
- `rep_weights_live.bin` (currently 6,544 bytes of Nano network data)
- `rep_weights_beta.bin` (currently 736 bytes of Nano network data)

### Deleted (additional)
- `etc/systemd/nanocurrency-beta.service` (no beta network)

### Renamed
- `nanocurrency.spec.in` → `kakitu.spec.in`
- `etc/systemd/nanocurrency.service` → `etc/systemd/kakitu.service`
- `etc/systemd/nanocurrency-test.service` → `etc/systemd/kakitu-test.service`

---

## Critical Operational Checklist (Before Launch)

- [ ] Genesis private key generated and stored securely offline
- [ ] Genesis open block constructed, work verified, signature verified
- [ ] Canary private keys (live + beta) stored securely (needed for future epoch upgrades)
- [ ] Epoch v2 signer private key stored securely offline
- [ ] `peering.kakitu.org` DNS A record pointing to genesis node server
- [ ] Genesis node server firewall: ports `44075/44076/44077/44078` open
- [ ] Genesis node built with new genesis block and running before any other nodes attempt to join
- [ ] `rep_weights_live.bin` and `rep_weights_beta.bin` confirmed zero-byte on the genesis node
