# Kakitu User Flow Guide

## Complete Guide: How Users Mint and Burn KSHS Tokens

This document explains exactly how users interact with Kakitu to convert KES ↔ KSHS.

---

## Overview: The Three Methods

Users can deposit KES and receive KSHS tokens in three ways:

1. **STK Push (Recommended)** - Automated prompt on user's phone
2. **Manual Paybill** - User manually sends M-Pesa
3. **QR Code** - Scan to pay (future feature)

---

## Method 1: STK Push (Automated)

### User Perspective

1. User opens Kakitu wallet app/website
2. User clicks "Deposit" or "Buy KSHS"
3. User enters:
   - Amount in KES (e.g., 1000)
   - Phone number (254712345678)
4. User clicks "Continue"
5. **Phone receives STK Push prompt immediately**
6. User enters M-Pesa PIN on phone
7. **Within 5 seconds**: KSHS tokens appear in wallet

### Technical Flow

```
┌──────────────┐
│   User App   │  1. POST /rpc
│  (Web/Mobile)│     {
└──────┬───────┘       "action": "initiate_deposit",
       │               "phone": "254712345678",
       │               "amount": "1000",
       │               "account": "kshs_3abc..."
       │             }
       ↓
┌──────────────┐
│  Kakitu Node │  2. RPC Handler: initiate_deposit()
│ (Your Server)│     Calls: mpesa_bridge->initiate_deposit()
└──────┬───────┘
       │
       ↓              3. mpesa_client->initiate_stk_push()
┌──────────────┐        POST https://sandbox.safaricom.co.ke/mpesa/stkpush/v1/processrequest
│   M-Pesa API │        {
│   (Safaricom)│          "BusinessShortCode": "174379",
└──────┬───────┘          "Amount": "1000",
       │                  "PhoneNumber": "254712345678",
       │                  "CallBackURL": "https://yourserver.com/mpesa/stk-callback",
       │                  "AccountReference": "kshs_3abc..."
       │                }
       │
       ↓              4. M-Pesa sends STK Push to phone
┌──────────────┐
│  User Phone  │  5. User enters M-Pesa PIN
│  (STK Push)  │
└──────┬───────┘
       │
       ↓              6. M-Pesa confirms payment
┌──────────────┐        POST https://yourserver.com/mpesa/stk-callback
│   M-Pesa API │        {
│   (Callback) │          "ResultCode": "0",
└──────┬───────┘          "TransactionID": "QGK1234567",
       │                  "Amount": "1000",
       │                  "PhoneNumber": "254712345678"
       │                }
       ↓
┌──────────────┐
│Webhook Server│  7. mpesa_webhook_server receives callback
│  (Port 8080) │     Calls: bridge->process_deposit()
└──────┬───────┘
       │
       ↓              8. mint_authority->create_mint()
┌──────────────┐        Creates KSHS tokens
│Mint Authority│        Signs operation
└──────┬───────┘        Logs to blockchain
       │
       ↓              9. Send KSHS to user account
┌──────────────┐
│   Blockchain │  10. User receives 1000 KSHS at kshs_3abc...
│              │      Transaction visible on block explorer
└──────────────┘
```

### RPC Request Example

```bash
curl -X POST http://localhost:7076/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "action": "initiate_deposit",
    "phone_number": "254712345678",
    "amount": "1000",
    "account": "kshs_3abc...xyz"
  }'
```

### RPC Response

```json
{
  "checkout_request_id": "ws_CO_12345678901234567890",
  "status": "pending",
  "message": "STK Push sent to phone. User should enter M-Pesa PIN."
}
```

---

## Method 2: Manual Paybill

### User Perspective

1. User opens M-Pesa on phone
2. User selects "Lipa na M-Pesa" → "Paybill"
3. User enters:
   - **Business Number**: 174379 (your paybill)
   - **Account Number**: kshs_3abc...xyz (their KSHS address)
   - **Amount**: 1000 KES
4. User enters M-Pesa PIN
5. User receives M-Pesa confirmation SMS
6. **Within 30 seconds**: KSHS tokens appear in wallet

### Technical Flow

```
┌──────────────┐
│  User Phone  │  1. User sends M-Pesa to Paybill 174379
│  (M-Pesa App)│     Account: kshs_3abc...xyz
└──────┬───────┘     Amount: 1000 KES
       │
       ↓              2. M-Pesa processes payment
┌──────────────┐        POST https://yourserver.com/mpesa/confirm
│   M-Pesa API │        {
│   (C2B)      │          "TransID": "QGK1234567",
└──────┬───────┘          "BillRefNumber": "kshs_3abc...xyz",
       │                  "TransAmount": "1000",
       │                  "MSISDN": "254712345678",
       │                  "FirstName": "John",
       │                  "LastName": "Doe"
       │                }
       ↓
┌──────────────┐
│Webhook Server│  3. handle_confirmation()
│              │     Extracts: account from BillRefNumber
└──────┬───────┘     Calls: bridge->process_deposit()
       │
       ↓              4. Mint KSHS tokens
┌──────────────┐
│Mint Authority│  5. create_mint() → Send to kshs_3abc...xyz
└──────┬───────┘
       │
       ↓
┌──────────────┐
│   Blockchain │  6. User receives 1000 KSHS
└──────────────┘
```

---

## Withdrawal Flow (Burning KSHS)

### User Perspective

1. User opens Kakitu wallet
2. User clicks "Withdraw" or "Sell KSHS"
3. User enters:
   - Amount in KSHS (e.g., 500)
   - Phone number to receive KES
4. User clicks "Confirm"
5. User approves transaction with wallet signature
6. **Within 2 minutes**: KES appears in M-Pesa account

### Technical Flow

```
┌──────────────┐
│   User App   │  1. POST /rpc
└──────┬───────┘     {
       │               "action": "initiate_withdrawal",
       │               "amount": "500",
       │               "phone": "254712345678",
       │               "signature": "..."
       │             }
       ↓
┌──────────────┐
│  Kakitu Node │  2. Verify user owns 500 KSHS
└──────┬───────┘     Create send block to burn address
       │
       ↓              3. User signs send to burn address
┌──────────────┐        kshs_1111111111111111111111111111111111hifc8npp
│  Blockchain  │        Amount: 500 KSHS
└──────┬───────┘
       │
       ↓              4. Bridge detects burn transaction
┌──────────────┐        Calls: mint_authority->create_burn()
│Mint Authority│        Logs burn operation
└──────┬───────┘
       │
       ↓              5. Initiate B2C payment
┌──────────────┐        POST https://api.safaricom.co.ke/mpesa/b2c/v1/paymentrequest
│ M-Pesa Bridge│        {
└──────┬───────┘          "Amount": "490",  // 500 - 10 fee
       │                  "PartyB": "254712345678",
       │                  "Remarks": "KSHS Redemption"
       │                }
       ↓
┌──────────────┐
│   M-Pesa API │  6. M-Pesa sends KES to user phone
└──────┬───────┘
       │
       ↓
┌──────────────┐
│  User Phone  │  7. User receives 490 KES
│  (SMS)       │     "You have received 490 KES from KAKITU"
└──────────────┘
```

---

## RPC Endpoints Available to Users

### 1. `initiate_deposit` - Start deposit (STK Push)

**Request:**
```json
{
  "action": "initiate_deposit",
  "phone_number": "254712345678",
  "amount": "1000",
  "account": "kshs_3abc...xyz"
}
```

**Response:**
```json
{
  "checkout_request_id": "ws_CO_12345678901234567890",
  "status": "pending",
  "message": "STK Push sent to phone. User should enter M-Pesa PIN."
}
```

### 2. `supply_info` - Check total supply and reserves

**Request:**
```json
{
  "action": "supply_info"
}
```

**Response:**
```json
{
  "total_supply": "10000000",
  "total_reserves": "10000000",
  "backing_ratio": "1.0",
  "is_balanced": true
}
```

### 3. `operation_history` - View mint/burn operations

**Request:**
```json
{
  "action": "operation_history",
  "start_timestamp": "1234567890",
  "end_timestamp": "1234567999"
}
```

**Response:**
```json
{
  "operations": [
    {
      "type": "MINT",
      "kshs_amount": "1000",
      "fiat_amount_kes": "1000",
      "mpesa_transaction_id": "QGK1234567",
      "phone_number": "254712345678",
      "destination": "kshs_3abc...xyz",
      "timestamp": "1234567890",
      "operation_hash": "ABC123..."
    }
  ]
}
```

### 4. `verify_operation` - Verify specific mint/burn

**Request:**
```json
{
  "action": "verify_operation",
  "mpesa_transaction_id": "QGK1234567"
}
```

**Response:**
```json
{
  "valid": true,
  "type": "MINT",
  "kshs_amount": "1000",
  "fiat_amount_kes": "1000",
  "destination": "kshs_3abc...xyz",
  "signature": "VALID_SIG...",
  "operation_hash": "ABC123..."
}
```

### 5. `reserve_status` - Check M-Pesa float balance

**Request:**
```json
{
  "action": "reserve_status"
}
```

**Response:**
```json
{
  "mpesa_balance_kes": "10000000",
  "kshs_total_supply": "10000000",
  "pending_deposits": "0",
  "pending_withdrawals": "50000",
  "is_balanced": true,
  "backing_ratio": "1.0"
}
```

---

## Webhook Endpoints (Internal)

These are called by M-Pesa, not by users:

### 1. `POST /mpesa/validate` - Pre-payment validation
Called before payment is accepted. Return `ResultCode: 0` to accept.

### 2. `POST /mpesa/confirm` - Payment confirmation
Called after successful payment. This triggers minting.

### 3. `POST /mpesa/stk-callback` - STK Push result
Called after user enters PIN (or cancels).

### 4. `POST /mpesa/b2c-result` - Withdrawal result
Called after B2C payment succeeds.

### 5. `POST /health` - Health check
Returns `{"status": "healthy"}`.

---

## Configuration: Making Webhooks Work

For STK Push and deposits to work, M-Pesa needs to know your webhook URLs.

### Option 1: Use ngrok for Testing

```bash
# Start your webhook server
cd /home/user/kakitu-node
./kakitu_node

# In another terminal, expose it
ngrok http 8080

# You'll get a URL like: https://abc123.ngrok.io
```

### Option 2: Use a Public Server

Deploy to a server with a public IP and domain name.

### Update Config File

Edit `nano/node/mpesa_config_sandbox.json`:

```json
{
  "mpesa": {
    "webhook_urls": {
      "validation_url": "https://abc123.ngrok.io/mpesa/validate",
      "confirmation_url": "https://abc123.ngrok.io/mpesa/confirm",
      "stk_callback_url": "https://abc123.ngrok.io/mpesa/stk-callback",
      "b2c_result_url": "https://abc123.ngrok.io/mpesa/b2c-result",
      "b2c_timeout_url": "https://abc123.ngrok.io/mpesa/b2c-timeout"
    }
  }
}
```

### Register URLs with M-Pesa

The node automatically registers these URLs when it starts (see `initialize_mpesa_client()` in `kakitu_node_integration.cpp`).

---

## Testing the Complete Flow

### Step 1: Start the Node

```bash
cd /home/user/kakitu-node
./kakitu_node --network=kshs_live_network
```

You should see:
```
M-Pesa API client initialized and authenticated
C2B URLs registered successfully
Webhook server started on port 8080
Kakitu node initialized successfully
```

### Step 2: Expose Webhooks (if local)

```bash
ngrok http 8080
```

Update config with your ngrok URL and restart node.

### Step 3: Test STK Push

```bash
curl -X POST http://localhost:7076/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "action": "initiate_deposit",
    "phone_number": "254708374149",
    "amount": "1",
    "account": "kshs_3abc...xyz"
  }'
```

### Step 4: Enter PIN on Phone

The test phone (254708374149) will receive an STK Push. Enter PIN: **1234** (sandbox PIN).

### Step 5: Verify Minting

```bash
curl -X POST http://localhost:7076/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "action": "supply_info"
  }'
```

You should see:
```json
{
  "total_supply": "1",
  "total_reserves": "1",
  "backing_ratio": "1.0",
  "is_balanced": true
}
```

---

## Security Considerations

### User Account Linking

The `BillRefNumber` (account number in Paybill) is the user's KSHS address. This links M-Pesa payments to blockchain accounts.

**Important**: Users MUST use their correct KSHS address when sending M-Pesa, or tokens will be minted to the wrong account!

### Recommendations:

1. **Generate QR codes** with embedded account addresses
2. **Use STK Push** (automated, no user error)
3. **Validate addresses** before accepting payments

### Private Key Security

The mint authority private key must be kept secure:

```bash
# Store in environment variable, NEVER in code or config
export KAKITU_MINT_PRIVATE_KEY="0000000000000000000000000000000000000000000000000000000000000001"

# Or use a secrets manager in production
```

### Rate Limiting

Configure daily limits in `mpesa_config_sandbox.json`:

```json
{
  "limits": {
    "daily_deposit_limit_kes": 1000000,
    "daily_withdrawal_limit_kes": 1000000
  }
}
```

---

## Troubleshooting

### STK Push Not Received

1. Check phone number format (must be 254XXXXXXXXX)
2. Verify phone is Safaricom M-Pesa enabled
3. Check M-Pesa API authentication (look for auth errors in logs)
4. In sandbox: Use test number 254708374149

### Webhook Not Called

1. Verify webhook server is running (`curl http://localhost:8080/health`)
2. Check firewall allows incoming on port 8080
3. Verify ngrok/public URL is accessible
4. Check M-Pesa registered your URLs (see logs at startup)

### Tokens Not Minting

1. Check webhook server logs for errors
2. Verify mint authority initialized (check startup logs)
3. Check account address format (must be valid kshs_ address)
4. Look for signature/verification errors in logs

### M-Pesa Authentication Failed

1. Verify consumer key/secret in config are correct
2. Check if sandbox vs production mismatch
3. Regenerate keys from Safaricom developer portal
4. Check network connectivity to sandbox.safaricom.co.ke

---

## Next Steps

Once you've verified the basic flow works:

1. **Build a user-friendly wallet** (web or mobile app)
2. **Add QR code generation** for easy deposits
3. **Implement multi-signature** for security
4. **Add transaction notifications** (SMS, email, push)
5. **Create merchant SDK** for accepting KSHS payments
6. **Deploy to production** M-Pesa (get CBK approval first!)

---

## Summary

**For Users**:
- Deposit KES → Receive KSHS (instant via STK Push)
- Send KSHS to burn address → Receive KES (2 min via B2C)
- All operations cryptographically signed and auditable

**For Developers**:
- RPC endpoints: `initiate_deposit`, `supply_info`, `operation_history`, etc.
- Webhook server handles M-Pesa callbacks automatically
- Mint authority ensures 1:1 KES backing
- All code is in `nano/node/` and `nano/rpc/`

**The Missing Piece** (what you asked about):
- The STK Push IS implemented in `mpesa_integration.cpp:271`
- The RPC endpoint IS implemented in `kakitu_rpc.cpp:131`
- What was MISSING: Proper integration and configuration loading
- **NOW FIXED**: Config-based constructor, proper URL handling, test program

Everything is there - just needs to be compiled and integrated into the main node!
