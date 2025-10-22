# Kakitu Node - Quick Start Guide

## 🎯 What You Have Now

A **complete M-Pesa backed cryptocurrency node** with:

✅ **Nano Protocol Fork** - Fast, feeless transactions
✅ **Custom KSHS Token** - Your own cryptocurrency (1 KSHS = 1 KES)
✅ **M-Pesa Integration** - Full deposit/withdrawal architecture
✅ **Mint/Burn System** - Elastic supply matching reserves
✅ **Cryptographic Proofs** - Complete audit trail
✅ **Sandbox Ready** - Test configuration included

---

## 📁 Important Files Created

### Core Integration
- **`nano/node/mpesa_integration.hpp`** - Complete M-Pesa API client
- **`nano/node/mint_authority.hpp`** - Mint/burn authority system
- **`nano/node/mpesa_config_sandbox.json`** - Sandbox test configuration

### Documentation
- **`KAKITU_ARCHITECTURE.md`** - Complete system architecture
- **`MPESA_INTEGRATION_GUIDE.md`** - Detailed integration guide
- **`MPESA_SANDBOX_TESTING.md`** - Testing procedures with sandbox
- **`QUICK_START.md`** - This file

### Tools
- **`scripts/test_mpesa_connection.sh`** - M-Pesa connectivity test

---

## 🚀 Getting Started in 5 Steps

### Step 1: Get M-Pesa Credentials

**Option A: Use Sandbox (Testing)**
```bash
# Visit Safaricom Daraja Portal
https://developer.safaricom.co.ke/

# Sign up / Log in
# Create an app
# Get your credentials:
# - Consumer Key
# - Consumer Secret
# - Shortcode
# - Passkey
```

**Option B: Request Production Access**
- Email: apisupport@safaricom.co.ke
- Provide business registration documents
- Get assigned shortcode and credentials

### Step 2: Update Configuration

```bash
# Copy sandbox config
cp nano/node/mpesa_config_sandbox.json /etc/kakitu/config.json

# Edit with your credentials
nano /etc/kakitu/config.json

# Update:
# - consumer_key
# - consumer_secret
# - shortcode (if you have your own)
# - webhook URLs (use your domain)
```

### Step 3: Generate Mint Authority Keys

```bash
# Build the node first
cmake . -DACTIVE_NETWORK=kshs_live_network
make -j$(nproc)

# Generate keypair
./kakitu_node --wallet_create

# Create account
./kakitu_node --account_create --wallet=<wallet_id>

# IMPORTANT: Save the seed securely!
# This is your mint authority key

# Export private key (for development)
export KAKITU_MINT_PRIVATE_KEY="your_64_char_hex_key_here"

# For production: Use Hardware Security Module (HSM)
```

### Step 4: Set Genesis Supply to Zero

Edit `nano/secure/common.cpp` line 116:

```cpp
// Change from:
genesis_amount{ std::numeric_limits<nano::uint128_t>::max () },

// To:
genesis_amount{ nano::uint128_t("0") },  // Start with zero supply!
```

Rebuild:
```bash
cmake . -DACTIVE_NETWORK=kshs_live_network
make -j$(nproc)
```

### Step 5: Start Testing

```bash
# Start node
./kakitu_node --daemon

# In another terminal, test M-Pesa connection
./scripts/test_mpesa_connection.sh

# If using sandbox, use test phone: 254708374149
```

---

## 💰 Testing Deposit Flow

### Method 1: STK Push (Recommended)

```bash
# Initiate payment prompt on user's phone
# (Implementation in mpesa_integration.hpp)

# User will:
# 1. Receive M-Pesa prompt
# 2. Enter PIN
# 3. Confirm payment
# 4. Your system receives callback
# 5. System mints KSHS
# 6. Sends to user's wallet
```

### Method 2: Manual Paybill

```
User does:
1. Open M-Pesa menu
2. Select "Lipa na M-Pesa"
3. Select "Paybill"
4. Enter Business Number: YOUR_SHORTCODE
5. Enter Account Number: their_kshs_address
6. Enter Amount: e.g., 1000 KES
7. Enter PIN
8. Confirm

System does:
1. Receives C2B callback
2. Validates transaction
3. Mints 1000 KSHS
4. Sends to their_kshs_address
5. Logs with M-Pesa receipt
```

---

## 💸 Testing Withdrawal Flow

```bash
# User burns KSHS
# 1. Send KSHS to burn address: kshs_1111...hifc8npp
# 2. System detects burn
# 3. System sends KES via M-Pesa B2C
# 4. User receives money on phone
```

---

## 🧪 Sandbox Testing with webhook.site

Since M-Pesa needs public HTTPS URLs for callbacks:

```bash
# 1. Go to https://webhook.site
# 2. Copy your unique URL (e.g., https://webhook.site/abc123)
# 3. Update config.json:

{
  "webhook_urls": {
    "confirmation_url": "https://webhook.site/abc123/confirm",
    "validation_url": "https://webhook.site/abc123/validate",
    "stk_callback_url": "https://webhook.site/abc123/stk"
  }
}

# 4. Register URLs with M-Pesa
# 5. Test payment flows
# 6. Watch callbacks arrive in real-time on webhook.site
```

---

## 📊 Checking Reserves

```bash
# Query supply and reserves
curl http://localhost:7076 -d '{
  "action": "supply_info"
}'

# Expected response:
{
  "total_supply": "50000000000000000000000000000000000",
  "total_reserves": "50000000000000000000000000000000000",
  "backing_ratio": "1.0",
  "is_balanced": true
}
```

---

## 🔐 Security Checklist

Before going live:

- [ ] **Mint authority key in HSM** - Never expose private key
- [ ] **Webhook URLs use HTTPS** - Valid SSL certificate required
- [ ] **IP whitelist enabled** - Only allow Safaricom IPs
- [ ] **Rate limits configured** - Prevent abuse
- [ ] **Multi-sig for large amounts** - Require multiple approvals >100K
- [ ] **Monitoring setup** - Alert on failures/mismatches
- [ ] **Backup strategy** - Regular backups of operation log
- [ ] **Disaster recovery plan** - Document recovery procedures
- [ ] **Penetration testing** - Security audit completed
- [ ] **Legal compliance** - Regulatory requirements met

---

## 🐛 Troubleshooting

### M-Pesa Returns "Access Denied"

**Problem:** OAuth token request fails

**Solutions:**
1. Verify consumer key/secret correct
2. Check if you're using sandbox URL for sandbox credentials
3. Credentials may have expired - get new ones from Daraja portal
4. Network firewall blocking Safaricom IPs

```bash
# Test OAuth manually
curl -u "KEY:SECRET" \
  "https://sandbox.safaricom.co.ke/oauth/v1/generate?grant_type=client_credentials"
```

### Webhooks Not Receiving Callbacks

**Problem:** M-Pesa not calling your URLs

**Solutions:**
1. URLs must be publicly accessible HTTPS
2. Use webhook.site for testing
3. Use ngrok to tunnel localhost: `ngrok http 8080`
4. Register URLs with M-Pesa first
5. Check Safaricom firewall not blocking

### KSHS Not Minting After Deposit

**Problem:** Money received but no tokens created

**Solutions:**
1. Check mint authority key loaded correctly
2. Verify node RPC running: `curl http://localhost:7076`
3. Check operation log: `cat data/operations.log`
4. Ensure callback handler running
5. Check for errors in node logs

### Reserve Mismatch

**Problem:** Supply ≠ M-Pesa balance

**Solutions:**
1. Run reconciliation: `./kakitu_node --reconcile`
2. Check for failed M-Pesa transactions
3. Verify all operations logged
4. Query M-Pesa for recent transactions
5. Look for duplicate transaction processing

---

## 📞 Getting Help

### M-Pesa Support
- Email: apisupport@safaricom.co.ke
- Portal: https://developer.safaricom.co.ke
- Hours: Mon-Fri, 8 AM - 5 PM EAT

### Kakitu Development
- GitHub: https://github.com/kakitucurrency/kakitu-node
- Issues: Report bugs and request features

### Nano Protocol
- Docs: https://docs.nano.org
- Discord: https://chat.nano.org

---

## 📚 Additional Resources

1. **KAKITU_ARCHITECTURE.md** - Complete system design and flows
2. **MPESA_INTEGRATION_GUIDE.md** - Detailed M-Pesa integration
3. **MPESA_SANDBOX_TESTING.md** - Testing procedures and examples
4. **changes.md** - List of all code changes from Nano

---

## 🎯 Production Deployment Checklist

### Pre-Launch (2-4 weeks before)

- [ ] Complete sandbox testing
- [ ] Security audit completed
- [ ] Load testing (1000+ tx/day)
- [ ] Documentation complete
- [ ] User guides written
- [ ] Support system ready
- [ ] Monitoring/alerting configured
- [ ] Legal/regulatory approval
- [ ] Insurance/risk management
- [ ] Marketing materials prepared

### Launch Week

- [ ] Production credentials obtained
- [ ] Production server configured
- [ ] SSL certificates installed
- [ ] DNS configured
- [ ] Backups automated
- [ ] Monitoring active
- [ ] Team trained
- [ ] Support hotline ready
- [ ] Soft launch with 50 users
- [ ] Daily monitoring

### Post-Launch (First Month)

- [ ] Daily reconciliation
- [ ] User feedback collection
- [ ] Bug fixes deployed
- [ ] Performance optimization
- [ ] Feature improvements
- [ ] Marketing campaigns
- [ ] Partnerships established
- [ ] Media coverage

---

## 💡 Business Model Recommendations

### Phase 1: Bootstrap (Month 1-3)
```
Model: Withdrawal fees only
- Deposits: FREE
- Withdrawals: 10 KES flat fee
- Target: 1000 users, 100K KES daily volume
- Monthly revenue: ~30K KES
```

### Phase 2: Growth (Month 4-12)
```
Model: Add exchange spread
- Deposits: 0.5% fee (optional)
- Withdrawals: 0.5% fee or 10 KES
- Float interest: 10% annual on reserves
- Target: 10,000 users, 1M KES daily volume
- Monthly revenue: ~200K KES
```

### Phase 3: Scale (Year 2+)
```
Model: Premium features
- Basic: Free
- Premium: 500 KES/month
  - Higher limits
  - Instant withdrawals
  - API access
- Business: 5000 KES/month
  - Merchant payments
  - Bulk operations
  - Dedicated support
- Target: 100,000 users
- Monthly revenue: 2M+ KES
```

---

## 🎉 You're Ready!

You now have everything needed to build a production M-Pesa backed cryptocurrency:

✅ Complete codebase architecture
✅ M-Pesa integration framework
✅ Security best practices
✅ Testing procedures
✅ Documentation

**Next step:** Get M-Pesa credentials and start testing!

Good luck with Kakitu! 🚀
