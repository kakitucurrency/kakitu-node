# Kakitu - Kenya's M-Pesa Backed Cryptocurrency

![Kakitu Logo](https://via.placeholder.com/200x80/4CAF50/FFFFFF?text=KAKITU)

## 🇰🇪 Digital Shillings for the Digital Economy

Kakitu (KSHS) is Kenya's first M-Pesa-integrated cryptocurrency, providing a stable, 1:1 KES-backed digital currency with instant settlement and zero fees.

---

## ⚡ Key Features

- **1:1 KES Backing**: Every KSHS token backed by Kenyan Shillings in M-Pesa
- **Instant Transactions**: Sub-second finality
- **Zero Fees**: No transaction costs for users
- **M-Pesa Integration**: Direct deposits and withdrawals via M-Pesa
- **Elastic Supply**: Mint-on-buy, burn-on-sell model
- **Decentralized**: Distributed network of nodes across Kenya
- **Revenue Sharing**: 50% of operational revenue to node operators

---

## 🚀 Quick Start

### For Users

1. **Deposit via M-Pesa**
   ```
   Send KES via STK Push → Receive KSHS tokens
   ```

2. **Transfer KSHS**
   ```
   Send to any KSHS address instantly
   ```

3. **Withdraw to M-Pesa**
   ```
   Burn KSHS tokens → Receive KES in M-Pesa wallet
   ```

### For Node Operators

See [NODE_OPERATOR_GUIDE.md](NODE_OPERATOR_GUIDE.md) for complete setup instructions.

**Quick Deploy:**
```bash
# Build
git clone https://github.com/kakitucurrency/kakitu-node.git
cd kakitu-node
cmake . -DACTIVE_NETWORK=kshs_live_network
make nano_node -j$(nproc)

# Deploy
sudo tools/deploy_node.sh

# Generate keys
tools/generate_mint_authority.sh

# Start
sudo systemctl start kakitu-node
```

---

## 🏗️ Architecture

### Network Structure

```
┌─────────────────────────────────────────────────────┐
│                  Kakitu Network                      │
│                                                       │
│  ┌───────────┐    ┌───────────┐    ┌───────────┐   │
│  │  Node 1   │────│  Node 2   │────│  Node 3   │   │
│  │ (Nairobi) │    │ (Mombasa) │    │ (Kisumu)  │   │
│  └─────┬─────┘    └─────┬─────┘    └─────┬─────┘   │
│        │                │                │          │
│  ┌─────▼────────────────▼────────────────▼─────┐   │
│  │         Distributed Ledger (Blockchain)      │   │
│  └──────────────────────┬────────────────────────┘  │
│                         │                            │
└─────────────────────────┼────────────────────────────┘
                          │
                 ┌────────▼────────┐
                 │  M-Pesa  API   │
                 │  Integration    │
                 └─────────────────┘
                          │
                 ┌────────▼────────┐
                 │   Safaricom     │
                 │   M-Pesa        │
                 └─────────────────┘
```

### Transaction Flow

```
User Deposit (KES → KSHS):
1. User initiates deposit via RPC/app
2. Node sends STK Push to user's phone
3. User enters M-Pesa PIN
4. M-Pesa confirms payment via webhook
5. Node mints equivalent KSHS tokens
6. Tokens sent to user's KSHS address

User Withdrawal (KSHS → KES):
1. User burns KSHS tokens
2. Node initiates B2C transfer
3. KES sent to user's M-Pesa number
4. Transaction logged on blockchain
```

---

## 💰 Economics

### Supply Model

- **Initial Supply**: 0 KSHS
- **Max Supply**: Unlimited (limited by KES reserves)
- **Issuance**: Mint-on-buy (backed 1:1)
- **Destruction**: Burn-on-sell
- **Reserve Ratio**: 100% (full backing)

### Revenue Model

**Transaction Fees**: Paid by M-Pesa (not users)
- M-Pesa charges ~1% per transaction
- 50% goes to node operators (by voting weight)
- 30% goes to operational reserve
- 20% goes to development/marketing

**Node Operator Rewards**:
- Base reward: Proportional to voting weight
- Uptime bonus: +10% for 99.5%+ uptime
- Geographic bonus: +20% for Principal Nodes
- Performance bonus: +5% for fast webhook response

---

## 🛠️ Technology Stack

- **Base Protocol**: Nano (modified)
- **Consensus**: Open Representative Voting (ORV)
- **Address Prefix**: `kshs_` (Kenya Shillings)
- **Block Lattice**: Individual chain per account
- **Payment Integration**: Safaricom M-Pesa API
- **Smart Contracts**: Mint authority + Burn mechanism
- **Language**: C++ (core), REST API (webhooks)

---

## 📊 Network Statistics

### Mainnet (Live)
- **Network**: `kshs_live_network`
- **RPC Port**: 7076
- **P2P Port**: 7075
- **Webhook Port**: 8080

### Performance Metrics
- **Transaction Speed**: < 1 second finality
- **Throughput**: 1000+ TPS capable
- **Network Uptime**: 99.9% target
- **M-Pesa Settlement**: 2-60 seconds

---

## 🔐 Security

### Network Security
- Multi-signature mint authority (optional)
- Distributed trust across nodes
- No single point of failure
- Cryptographic audit trail

### Fund Security
- 100% KES reserve backing
- Real-time reserve monitoring
- Monthly third-party audits
- Insurance coverage (planned)

### Operational Security
- Rate limiting on deposits/withdrawals
- Fraud detection algorithms
- Webhook signature verification
- DDoS protection

---

## 🧪 Testing

### Sandbox Environment

Test with M-Pesa sandbox:

```bash
# Use sandbox config
cp mpesa_config_sandbox.json kakitu_config.json

# Start node
./nano_node --network=kshs_test_network --data_path=./test_data
```

### RPC Testing

```bash
# Check supply
curl -X POST http://localhost:7076 -d '{
  "action": "supply_info"
}'

# Initiate test deposit
curl -X POST http://localhost:7076 -d '{
  "action": "initiate_deposit",
  "phone_number": "254708374149",
  "account": "kshs_1test...",
  "amount": "100000000000000000000000000000"
}'

# Check operation history
curl -X POST http://localhost:7076 -d '{
  "action": "operation_history",
  "count": "10"
}'
```

---

## 📚 Documentation

- [Node Operator Guide](NODE_OPERATOR_GUIDE.md) - Complete setup & operations
- [API Reference](docs/RPC_API.md) - RPC endpoints documentation
- [Integration Guide](docs/INTEGRATION.md) - For wallets & exchanges
- [Security Guide](docs/SECURITY.md) - Best practices
- [Development Guide](docs/DEVELOPMENT.md) - Contributing code

---

## 🤝 Contributing

We welcome contributions! See [CONTRIBUTING.md](CONTRIBUTING.md).

### Development Setup

```bash
# Clone repo
git clone https://github.com/kakitucurrency/kakitu-node.git
cd kakitu-node

# Install dependencies (Ubuntu)
sudo apt install -y build-essential cmake git \
    libboost-all-dev libcurl4-openssl-dev libssl-dev \
    libjsoncpp-dev pkg-config

# Build
cmake . -DACTIVE_NETWORK=kshs_test_network
make nano_node -j$(nproc)

# Run tests
make test
```

### Areas for Contribution

- Mobile wallet development (iOS/Android)
- Web wallet frontend
- Additional payment integrations
- Performance optimizations
- Documentation improvements
- Translation (Swahili, other languages)

---

## 🗺️ Roadmap

### Phase 1: Launch (Q4 2025) ✓
- [x] Custom node build with M-Pesa integration
- [x] Mainnet deployment
- [ ] Initial node operator onboarding (10+ nodes)
- [ ] Basic web wallet

### Phase 2: Growth (Q1 2026)
- [ ] Mobile wallets (iOS & Android)
- [ ] Merchant integration SDK
- [ ] Exchange listings
- [ ] 100+ active nodes across Kenya

### Phase 3: Scale (Q2 2026)
- [ ] Cross-border payments (Tanzania, Uganda)
- [ ] Merchant point-of-sale terminals
- [ ] Institutional adoption
- [ ] 1000+ daily active users

### Phase 4: Ecosystem (Q3 2026)
- [ ] Smart contracts / programmability
- [ ] DeFi integrations
- [ ] Bill payment integrations
- [ ] Government partnerships

---

## 💼 Use Cases

### For Individuals
- **Remittances**: Send money instantly, no fees
- **Savings**: Hold KES-backed stable digital currency
- **Online Payments**: Pay merchants without card fees
- **Peer-to-Peer**: Split bills, send gifts

### For Businesses
- **Merchant Payments**: Accept payments, settle instantly
- **Payroll**: Pay employees in KSHS
- **Invoicing**: Invoice in stable KES-backed currency
- **Cross-Border**: Trade with neighboring countries

### For Developers
- **Payment Integration**: Easy API for accepting KSHS
- **Wallet Services**: Build custom wallets
- **Exchange Services**: List KSHS trading pairs
- **DApp Development**: Build on Kakitu platform

---

## 📜 License

Kakitu Node is licensed under the [BSD 3-Clause License](LICENSE).

---

## 🆘 Support

- **Email**: support@kakitu.com
- **Telegram**: https://t.me/kakitu_support
- **Discord**: https://discord.gg/kakitu
- **Twitter**: [@KakituCurrency](https://twitter.com/KakituCurrency)
- **GitHub Issues**: [Report a bug](https://github.com/kakitucurrency/kakitu-node/issues)

---

## 👥 Team

**Kakitu Securities Limited**
- Registered in Kenya
- Licensed by Capital Markets Authority (pending)
- Backed by local and international investors

---

## ⚠️ Disclaimer

Kakitu (KSHS) is a digital representation of Kenyan Shillings backed by M-Pesa reserves. While every KSHS token is backed 1:1 by KES, cryptocurrency investments carry risk. Always do your own research. Not financial advice.

---

## 🌟 Why Kakitu?

1. **Built for Kenya**: Designed specifically for the Kenyan market
2. **M-Pesa Native**: Seamless integration with existing payment habits
3. **Zero Fees**: No transaction costs for users
4. **Instant**: Faster than bank transfers
5. **Stable**: 1:1 KES backing, no volatility
6. **Inclusive**: Anyone with a phone can participate
7. **Transparent**: Open-source, auditable reserves
8. **Community-Owned**: Node operators share in success

---

**Join the financial revolution. Run a node. Earn rewards. Build Kenya's digital future.**

```
npm install @kakitu/sdk
```

---

© 2025 Kakitu Securities Limited. All rights reserved.
