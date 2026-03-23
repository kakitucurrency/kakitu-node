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
| Network magic bytes | `KL` (live), `KD` (dev), `KX` (test) |

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

Replace entries:

| Field | Action |
|-------|--------|
| `live_public_key_data` | New genesis public key |
| `live_genesis_data` | New genesis open block JSON with `kshs_` addresses |
| `dev_public_key_data` / `dev_private_key_data` | Keep Nano dev key (publicly known, fine for local dev) |
| `dev_genesis_data` | Update `xrb_` addresses to `kshs_` prefix only |
| `beta_public_key_data` / `beta_genesis_data` | Remove (no beta network) |
| `test_public_key_data` / `test_genesis_data` | Keep env-var driven (unchanged) |

### 1.4 Generate fresh canary keypairs

Two new keypairs (you hold the private keys):
- `live_canary_public_key_data` — replaces `7CBAF192...`
- `beta_canary_public_key_data` — replaces `868C6A9F...` (kept in code even with no beta network, as the code path still references it)

### 1.5 Generate fresh epoch v2 signer keypair

Replace `kshs_3qb6o6i1tkzr6jwr5s7eehfxwg9x6eemitdinbpi7u8bjjwsgqfj4wzser3x` in `common.cpp:145` with a new keypair you control. This key authorises epoch v2 upgrades on the live network. Private key must be kept secure.

### 1.6 Clear rep_weights files

Replace `rep_weights_live.bin` and `rep_weights_beta.bin` with empty zero-byte files. The node builds its own weight table from the ledger at startup.

---

## Section 2 — Network Isolation

### 2.1 Network magic bytes (`nano/lib/config.hpp`)

```cpp
kshs_dev_network  = 0x4B44,  // 'K', 'D'
kshs_beta_network = 0x4B42,  // 'K', 'B'
kshs_live_network = 0x4B4C,  // 'K', 'L'
kshs_test_network = 0x4B58,  // 'K', 'X'
```

Ensures all Kakitu packets are immediately rejected by Nano nodes and vice versa.

### 2.2 Live network ports (`nano/lib/config.hpp`)

```cpp
default_node_port      = 44075;
default_rpc_port       = 44076;
default_ipc_port       = 44077;
default_websocket_port = 44078;
```

Dev network retains `44000/45000/46000/47000` (local-only, no conflict risk).

### 2.3 Data directory paths (`nano/secure/utility.cpp`)

```cpp
case nano::networks::kshs_dev_network:  result /= "KakituDev";  break;
case nano::networks::kshs_live_network: result /= "Kakitu";     break;
case nano::networks::kshs_test_network: result /= "KakituTest"; break;
```

### 2.4 Test magic number default (`nano/lib/config.cpp`)

```cpp
auto test_env = get_env_or_default ("KAKITU_TEST_MAGIC_NUMBER", "KX");
```

---

## Section 3 — Peering & Bootstrap Infrastructure

### 3.1 Default peer hostnames (`nano/node/nodeconfig.cpp`)

```cpp
std::string const default_live_peer_network =
    nano::get_env_or_default ("KAKITU_DEFAULT_PEER", "peering.kakitu.org");
std::string const default_test_peer_network =
    nano::get_env_or_default ("KAKITU_DEFAULT_PEER", "peering-test.kakitu.org");
```

Beta peer entry removed.

### 3.2 DNS setup (operational — not a code change)

Create DNS A record before launch:
```
peering.kakitu.org  →  <genesis node public IP>
```

### 3.3 Preconfigured representatives (`nano/node/nodeconfig.cpp`)

Live network case replaces 8 Nano Foundation rep keys with:
```cpp
case nano::networks::kshs_live_network:
    preconfigured_peers.emplace_back (default_live_peer_network);
    preconfigured_representatives.emplace_back (network_params.ledger.genesis->account ());
    break;
```

Beta network case removed entirely.

### 3.4 rep_weights files

`rep_weights_live.bin` and `rep_weights_beta.bin` truncated to zero bytes.

---

## Section 4 — Binary, Service & Packaging Rebrand

### 4.1 Binary names (`CMakeLists.txt` + subdirectory `CMakeLists.txt` files)

| Old | New |
|-----|-----|
| `nano_node` | `kakitu_node` |
| `nano_rpc` | `kakitu_rpc` |
| `nano_wallet` | `kakitu_wallet` |

All `add_executable()`, `install(TARGETS ...)`, and `target_link_libraries()` references updated.

### 4.2 CMake package metadata (`CMakeLists.txt`)

```cmake
project(kakitu-node)
set(CPACK_PACKAGE_NAME        "kakitu-node")
set(CPACK_PACKAGE_VENDOR      "Kakitu Currency")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "kakitu")
set(CPACK_NSIS_PACKAGE_NAME   "Kakitu")
set(CPACK_NSIS_URL_INFO_ABOUT "https://kakitu.org")
set(CPACK_NSIS_CONTACT        "info@kakitu.org")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "info@kakitu.org")
set(KAKITU_SERVICE            "kakitu.service")
```

### 4.3 Systemd service file

`etc/systemd/nanocurrency.service` → `etc/systemd/kakitu.service`
- Binary: `kakitu_node`
- Description: `Kakitu Currency Daemon`
- User/group: `kakitu`
- Home: `/var/kakitu/`

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

TOML config comment in `nodeconfig.cpp` updated to reference `KAKITU_DEFAULT_PEER`.
CLI config generation comment in `cli.cpp` updated to reference `kakitu.org`.

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
- `nano/secure/common.cpp` — genesis keys/blocks, canary keys, epoch v2 signer, env var names
- `nano/secure/utility.cpp` — data directory names
- `nano/lib/config.hpp` — network magic bytes, live ports
- `nano/lib/config.cpp` — env var names (test ports, magic number, PoW thresholds)
- `nano/node/nodeconfig.cpp` — peer hostnames, env var name, preconfigured reps
- `nano/node/cli.cpp` — config comment
- `nano/nano_node/daemon.cpp` — startup text
- `nano/nano_node/entry.cpp` — Windows event log key
- `CMakeLists.txt` — project name, binary names, package metadata, service name
- `nano/nano_node/CMakeLists.txt` — binary target name
- `nano/rpc/CMakeLists.txt` — binary target name
- `etc/systemd/nanocurrency.service` → `etc/systemd/kakitu.service`
- `.github/workflows/live_artifacts.yml` — repo default
- `.github/workflows/test_network_artifacts.yml` — repo default
- `SECURITY.md`, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md` — branding
- `.github/ISSUE_TEMPLATE/*.yml` — branding

### Deleted
- `.github/workflows/beta_artifacts.yml`
- `.github/workflows/beta_artifacts_latest_release_branch.yml`
- `nanocurrency-beta.spec.in`

### Replaced (zero-byte)
- `rep_weights_live.bin`
- `rep_weights_beta.bin`

### Renamed
- `nanocurrency.spec.in` → `kakitu.spec.in`

---

## Critical Operational Checklist (Before Launch)

- [ ] Genesis private key generated and stored securely offline
- [ ] Genesis open block constructed, work verified, signature verified
- [ ] `peering.kakitu.org` DNS A record pointing to genesis node server
- [ ] Genesis node server configured with ports `44075/44076/44077/44078` open in firewall
- [ ] Genesis node built and running with the new genesis block before any other nodes join
- [ ] Canary private keys stored securely (needed for future epoch upgrades)
- [ ] Epoch v2 signer private key stored securely
