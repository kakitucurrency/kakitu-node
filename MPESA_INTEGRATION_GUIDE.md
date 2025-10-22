# Kakitu M-Pesa Integration Guide

## 🎯 Overview

This guide explains how to integrate Kakitu with M-Pesa for seamless deposits and withdrawals.

**Model**: Mint-on-buy, Burn-on-sell
**Backing**: 1 KSHS = 1 KES (Kenyan Shilling)
**Method**: M-Pesa Paybill (deposits) + B2C (withdrawals)

---

## 📋 Prerequisites

### 1. M-Pesa Daraja API Access

Register at [Daraja Portal](https://developer.safaricom.co.ke/):

- Create an app
- Get Consumer Key and Consumer Secret
- Get your Paybill/Till Number (shortcode)
- Get Passkey for STK Push
- Set up Initiator credentials for B2C

### 2. Server Requirements

- Public HTTPS domain (required for M-Pesa callbacks)
- SSL certificate (Let's Encrypt works great)
- Open ports for webhook server
- Safaricom IP whitelist (see config)

### 3. Kakitu Node Setup

- Mint authority keypair generated
- Private key stored securely (HSM recommended)
- Node running with RPC enabled

---

## 🚀 Quick Start

### Step 1: Configure M-Pesa

```bash
# Copy example config
cp nano/node/mpesa_config_example.json /etc/kakitu/mpesa_config.json

# Edit with your credentials
nano /etc/kakitu/mpesa_config.json
```

Update:
- `consumer_key` and `consumer_secret`
- `shortcode` (your paybill number)
- `passkey` (from Daraja)
- All `webhook_urls` to your domain

### Step 2: Generate Mint Authority Keys

```bash
# Generate new keypair for mint authority
./kakitu_node --wallet_create

# Get the account and save it
./kakitu_node --account_create --wallet=<wallet_id>

# CRITICAL: Save the seed securely!
# Export private key to environment variable
export KAKITU_MINT_PRIVATE_KEY="your_private_key_here"
```

### Step 3: Set Genesis Supply to Zero

Edit `nano/secure/common.cpp` line 116:

```cpp
// Change from:
genesis_amount{ std::numeric_limits<nano::uint128_t>::max () },

// To:
genesis_amount{ nano::uint128_t("0") },  // Start with zero supply
```

Rebuild:

```bash
cmake . -DACTIVE_NETWORK=kshs_live_network
make -j$(nproc)
```

### Step 4: Start Webhook Server

```bash
# Start Kakitu node with M-Pesa integration
./kakitu_node --daemon --enable_mpesa_webhook

# Or run webhook server separately
./kakitu_mpesa_webhook --config /etc/kakitu/mpesa_config.json
```

### Step 5: Register Webhook URLs with M-Pesa

```bash
# Use the provided registration script
./scripts/register_mpesa_urls.sh
```

Or manually via API:

```bash
curl -X POST https://sandbox.safaricom.co.ke/mpesa/c2b/v1/registerurl \
  -H "Authorization: Bearer YOUR_ACCESS_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "ShortCode": "174379",
    "ResponseType": "Completed",
    "ConfirmationURL": "https://yourdomain.com/mpesa/confirm",
    "ValidationURL": "https://yourdomain.com/mpesa/validate"
  }'
```

---

## 💰 User Flow: Deposit (Buy KSHS)

### Method 1: Manual Paybill Payment

**User side:**
1. Open M-Pesa on phone
2. Select "Lipa na M-Pesa"
3. Select "Paybill"
4. Enter Business Number: `YOUR_SHORTCODE`
5. Enter Account Number: `THEIR_KSHS_ADDRESS`
6. Enter Amount: `1000` KES
7. Enter M-Pesa PIN
8. Confirm

**System side:**
1. M-Pesa sends callback to your confirmation URL
2. System validates transaction
3. Mint authority creates 1000 KSHS
4. Sends KSHS to user's address
5. Logs operation with M-Pesa receipt

### Method 2: STK Push (Automated)

**User visits your app/website:**
1. User enters KSHS address
2. User enters amount (e.g., 1000 KES)
3. System initiates STK Push
4. User receives M-Pesa prompt on phone
5. User enters PIN
6. System automatically mints and sends KSHS

**Implementation:**

```cpp
// User wants to buy 1000 KSHS with 1000 KES
auto checkout_id = mpesa_bridge->initiate_deposit(
    "254712345678",  // User's phone
    user_kakitu_account,
    nano::uint128_t("1000")  // KES amount
);

// Track the request
// When M-Pesa confirms, webhook handler will:
// 1. Verify payment
// 2. Mint KSHS
// 3. Send to user
```

---

## 💸 User Flow: Withdrawal (Sell KSHS)

**User side:**
1. User sends KSHS to burn address
2. User provides M-Pesa number for withdrawal
3. System detects burn transaction
4. M-Pesa sends KES to user's phone
5. User receives M-Pesa confirmation SMS

**System side:**

```cpp
// Detect burn transaction
if (block.destination == burn_address) {
    // Get user's registered phone number
    auto phone = get_phone_for_account(block.source);

    // Process withdrawal
    mpesa_bridge->process_withdrawal(
        block.hash(),
        phone,
        block.amount
    );

    // This will:
    // 1. Verify KSHS burned
    // 2. Send KES via B2C
    // 3. Log operation
}
```

---

## 🔒 Security Best Practices

### 1. Webhook Verification

Always verify M-Pesa callbacks:

```cpp
bool verify_mpesa_callback(Json::Value const & callback) {
    // Check IP address
    if (!is_safaricom_ip(request_ip)) {
        return false;
    }

    // Verify webhook secret
    if (callback["secret"] != webhook_secret) {
        return false;
    }

    // Query M-Pesa API to confirm transaction
    auto tx = mpesa_client->query_transaction(callback["TransID"]);
    if (tx.status != mpesa_status::COMPLETED) {
        return false;
    }

    return true;
}
```

### 2. Mint Authority Key Protection

```bash
# NEVER hardcode the private key
# Use environment variable
export KAKITU_MINT_PRIVATE_KEY="$(cat /secure/path/mint_key.txt)"

# Better: Use Hardware Security Module (HSM)
# Or AWS KMS / Google Cloud KMS

# Restrict file permissions
chmod 600 /secure/path/mint_key.txt
chown kakitu:kakitu /secure/path/mint_key.txt
```

### 3. Rate Limiting

```cpp
// Set daily limits
mint_authority->set_daily_mint_limit(
    nano::uint128_t("1000000") * nano::Mxrb_ratio  // 1M KSHS/day
);

mint_authority->set_daily_burn_limit(
    nano::uint128_t("1000000") * nano::Mxrb_ratio  // 1M KSHS/day
);
```

### 4. IP Whitelisting

Add to config:

```json
{
  "security": {
    "ip_whitelist": [
      "196.201.214.0/24",      // Safaricom
      "196.201.214.206",        // Safaricom callback server
      "YOUR_ADMIN_IP"
    ]
  }
}
```

---

## 📊 Monitoring & Reconciliation

### Daily Reconciliation

```bash
# Run daily at 2 AM
crontab -e

# Add:
0 2 * * * /usr/local/bin/kakitu_reconcile
```

Reconciliation checks:
- Total KSHS supply == M-Pesa balance
- All operations logged
- No orphaned transactions

### Real-time Monitoring

```cpp
// Check reserves every hour
auto status = mpesa_bridge->get_reserve_status();

if (!status.is_balanced) {
    // ALERT: Reserves don't match supply!
    send_alert("CRITICAL: Reserve mismatch!");

    // Log details
    logger.error("Supply: {}, Reserves: {}",
        status.kshs_total_supply,
        status.mpesa_balance_kes
    );
}
```

### Grafana Dashboard

Monitor:
- Total supply vs reserves (should be 1:1)
- Deposits per day
- Withdrawals per day
- M-Pesa float level
- Failed transactions
- Average processing time

---

## 🧪 Testing

### Sandbox Testing

M-Pesa provides a sandbox for testing:

```json
{
  "mpesa": {
    "environment": "sandbox",
    "consumer_key": "sandbox_key",
    "consumer_secret": "sandbox_secret",
    "shortcode": "174379"
  }
}
```

Test credentials:
- Test number: `254708374149`
- Test amount: Any amount
- Test PIN: Any 4 digits

### Integration Tests

```bash
# Run integration tests
./kakitu_test --test-mpesa-integration

# Test scenarios:
# 1. Successful deposit
# 2. Failed deposit (insufficient funds)
# 3. Successful withdrawal
# 4. Failed withdrawal (no float)
# 5. Duplicate transaction handling
# 6. Timeout handling
```

---

## 🐛 Troubleshooting

### Webhooks Not Receiving Callbacks

**Issue**: M-Pesa not calling your URLs

**Solutions**:
1. Verify domain is publicly accessible
2. Check SSL certificate is valid
3. Ensure URLs are registered with M-Pesa
4. Check Safaricom firewall not blocking

```bash
# Test webhook accessibility
curl https://yourdomain.com/mpesa/confirm

# Should return 200 OK
```

### Mint Operations Not Processing

**Issue**: Deposits received but KSHS not minted

**Solutions**:
1. Check mint authority key loaded
2. Verify node RPC is running
3. Check operation log for errors
4. Ensure sufficient work generation

```bash
# Check mint authority status
curl http://localhost:7076 -d '{
  "action": "mint_status"
}'
```

### Reserve Mismatch

**Issue**: KSHS supply ≠ M-Pesa balance

**Solutions**:
1. Run reconciliation report
2. Check for failed M-Pesa transactions
3. Verify all operations logged
4. Look for orphaned transactions

```bash
# Generate reconciliation report
./kakitu_node --generate-reconciliation-report \
  --start-date 2025-01-01 \
  --end-date 2025-01-31
```

---

## 📖 API Reference

### RPC Endpoints

#### Get Supply Info

```bash
curl http://localhost:7076 -d '{
  "action": "supply_info"
}'
```

Response:
```json
{
  "total_supply": "1000000000000000000000000000000000",
  "total_reserves": "1000000000000000000000000000000000",
  "backing_ratio": "1.0",
  "is_balanced": true
}
```

#### Get Operation History

```bash
curl http://localhost:7076 -d '{
  "action": "operation_history",
  "start_timestamp": 1640000000,
  "end_timestamp": 1640100000
}'
```

#### Verify Operation

```bash
curl http://localhost:7076 -d '{
  "action": "verify_operation",
  "mpesa_transaction_id": "QGK1234567"
}'
```

---

## 🚀 Production Deployment

### Pre-launch Checklist

- [ ] Mint authority keys generated and secured
- [ ] M-Pesa production credentials obtained
- [ ] Webhook URLs registered with Safaricom
- [ ] SSL certificates installed
- [ ] IP whitelist configured
- [ ] Rate limits set
- [ ] Monitoring/alerting set up
- [ ] Reconciliation process automated
- [ ] Backup strategy implemented
- [ ] Disaster recovery plan documented
- [ ] Regulatory compliance verified
- [ ] Security audit completed

### Launch Steps

1. **Test in Sandbox** (1 week)
   - Full flow testing
   - Edge case handling
   - Load testing

2. **Soft Launch** (2 weeks)
   - Limited user group
   - Daily reconciliation
   - Close monitoring

3. **Full Launch**
   - Open to public
   - Marketing campaign
   - 24/7 monitoring

### Scaling Considerations

- M-Pesa B2C has transaction limits (150K KES)
- Daily limits apply (check with Safaricom)
- May need multiple paybill numbers for scale
- Consider bulk disbursement for high withdrawal volume

---

## 📞 Support

- **M-Pesa Support**: apisupport@safaricom.co.ke
- **Kakitu Support**: support@kakitu.com
- **Documentation**: https://docs.kakitu.com

---

## 📄 License

This integration code is part of the Kakitu project and follows the same license as the main node.
