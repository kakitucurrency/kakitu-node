# Kakitu Node

[![Build Status](https://github.com/kakitucurrency/kakitu-node/workflows/Build/badge.svg)](https://github.com/kakitucurrency/kakitu-node/actions)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/kakitucurrency/kakitu-node)](https://github.com/kakitucurrency/kakitu-node/releases/latest)

---

### What is Kakitu?

Kakitu is a digital payment protocol designed to be accessible and lightweight, with a focus on removing inefficiencies present in other cryptocurrencies. With ultrafast transactions and zero fees on a secure, green and decentralized network, this makes Kakitu ideal for everyday transactions.

Kakitu uses the proven Nano protocol architecture with a custom address prefix (`kshs_`) while maintaining full backward compatibility with existing `xrb_` addresses.

---

### Key Features

* **Instant Transactions**: Transactions are confirmed in under 1 second
* **Zero Fees**: No transaction fees for sending or receiving Kakitu
* **Green**: Energy-efficient consensus mechanism with minimal environmental impact
* **Scalable**: Handles thousands of transactions per second
* **Decentralized**: No central authority or single point of failure
* **Custom Addresses**: Uses `kshs_` prefix for new addresses while supporting legacy `xrb_` addresses

---

### Building from Source

#### Prerequisites

* CMake 3.10+
* C++17 compatible compiler
* Boost libraries
* Git with submodules support

#### Build Instructions

```bash
# Clone the repository with submodules
git clone --recursive https://github.com/kakitucurrency/kakitu-node.git
cd kakitu-node

# Initialize and update submodules
git submodule update --init --recursive

# Configure and build
cmake . -DACTIVE_NETWORK=kshs_live_network
make -j$(nproc)
```

#### Build Products

* `kakitu_node` - Main node executable
* `kakitu_rpc` - RPC server executable

---

### Running a Node

```bash
# Run in daemon mode
./kakitu_node --daemon

# Create a new wallet
./kakitu_node --wallet_create

# Generate a new account
./kakitu_node --account_create --wallet=<wallet_id>

# Check account balance
./kakitu_node --account_balance --account=<kshs_address>
```

---

### Network Configuration

Kakitu supports multiple networks:

* **Live Network** (`kshs_live_network`) - Main production network
* **Beta Network** (`kshs_beta_network`) - Testing network for new features
* **Dev Network** (`kshs_dev_network`) - Development and debugging
* **Test Network** (`kshs_test_network`) - Automated testing

Use the `--network` flag to specify which network to connect to.

---

### Address Format

Kakitu introduces a new address format while maintaining compatibility:

* **New Format**: `kshs_` prefix for all newly generated addresses
* **Legacy Support**: Existing `xrb_` addresses are fully supported
* **Example**: `kshs_35zrk7ioxfeatbascyqx6iae5gzjbgb9yng4e56pjf8iix81y84j78u3k1ti`

---

### Documentation

* [Command Line Interface](https://docs.nano.org/commands/command-line-interface/)
* [RPC Protocol](https://docs.nano.org/commands/rpc-protocol/)
* [Running a Node](https://docs.nano.org/running-a-node/overview/)
* [Integration Guides](https://docs.nano.org/integration-guides/the-basics/)

*Note: Documentation currently references Nano implementation details, which also apply to Kakitu due to protocol compatibility.*

---

### Contributing

We welcome contributions to the Kakitu project! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

Please ensure all tests pass and follow the existing code style.

---

### Changes from Nano

This implementation includes the following modifications from the original Nano codebase:

* Address prefix changed from `nano_` to `kshs_`
* Network enum names updated to use `kshs_*_network` format
* Maintained full backward compatibility with `xrb_` addresses
* Updated branding and documentation

For detailed technical changes, see [changes.md](changes.md).

---

### License

Kakitu is released under the same license as Nano. See [LICENSE](LICENSE) for details.

---

### Support

For support, questions, or to report issues:

* File an issue on [GitHub](https://github.com/kakitucurrency/kakitu-node/issues)
* Join our community discussions

---

### Disclaimer

Kakitu is based on the Nano protocol and maintains protocol compatibility. This is an independent implementation with custom branding and address formatting.