# M-Pesa Sandbox Testing Guide

## 🧪 Quick Start with Sandbox Credentials

All sandbox credentials are pre-configured in `nano/node/mpesa_config_sandbox.json`.

---

## 📋 Sandbox Credentials (Public - Safe to Share)

```json
{
  "consumer_key": "9v38Dtu5u2BpsITPmLcXNWGMsjZRWSTG",
  "consumer_secret": "bclwIPMNRta40vT5",
  "shortcode": "174379",
  "passkey": "bfb279f9aa9bdbcf158e97dd71a467cd2e0c893059b10f78e6b72ada1ed2c919"
}
```

**Note:** These are Safaricom's official public sandbox credentials. Safe for testing!

---

## 📱 Test Phone Numbers

Safaricom provides these test numbers for sandbox:

```
Primary Test Number: 254708374149
Alternative Numbers: 254708374148, 254708374150

PIN: Any 4 digits (e.g., 1234)
```

**Important:** These numbers work ONLY in sandbox, not in production!

---

## 🚀 Quick Connection Test

### Method 1: Using the Test Script

```bash
# Run the connection test
cd /home/user/kakitu-node
./scripts/test_mpesa_connection.sh
```

Expected output:
```
✅ OAuth authentication: SUCCESS
✅ C2B URL registration: SUCCESS
✅ STK Push initiation: SUCCESS
🎉 M-Pesa Sandbox connection is working!
```

### Method 2: Manual cURL Test

```bash
# 1. Get OAuth Token
curl -X GET "https://sandbox.safaricom.co.ke/oauth/v1/generate?grant_type=client_credentials" \
  -H "Authorization: Basic $(echo -n 9v38Dtu5u2BpsITPmLcXNWGMsjZRWSTG:bclwIPMNRta40vT5 | base64)"

# Response should contain access_token
# Copy the token for next steps
```

---

## 🔄 Testing Complete Flows

### Flow 1: STK Push (Deposit Simulation)

This simulates a user depositing KES to buy KSHS.

```bash
# 1. Get access token (see above)
ACCESS_TOKEN="your_token_here"

# 2. Generate password
TIMESTAMP=$(date +"%Y%m%d%H%M%S")
PASSWORD=$(echo -n "174379bfb279f9aa9bdbcf158e97dd71a467cd2e0c893059b10f78e6b72ada1ed2c919$TIMESTAMP" | base64)

# 3. Initiate STK Push
curl -X POST "https://sandbox.safaricom.co.ke/mpesa/stkpush/v1/processrequest" \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  -H "Content-Type: application/json" \
  -d "{
    \"BusinessShortCode\": \"174379\",
    \"Password\": \"$PASSWORD\",
    \"Timestamp\": \"$TIMESTAMP\",
    \"TransactionType\": \"CustomerPayBillOnline\",
    \"Amount\": \"100\",
    \"PartyA\": \"254708374149\",
    \"PartyB\": \"174379\",
    \"PhoneNumber\": \"254708374149\",
    \"CallBackURL\": \"https://your-domain.com/mpesa/callback\",
    \"AccountReference\": \"kshs_1abc123xyz\",
    \"TransactionDesc\": \"Buy 100 KSHS\"
  }"

# Response will include CheckoutRequestID
# Save this to query status later
```

**What happens:**
1. M-Pesa sends prompt to phone 254708374149
2. User enters PIN (any 4 digits in sandbox)
3. M-Pesa confirms payment
4. Callback sent to your URL with transaction details

### Flow 2: C2B Payment (Manual Paybill)

This simulates user manually sending money via paybill.

```bash
# 1. Register your callback URLs
curl -X POST "https://sandbox.safaricom.co.ke/mpesa/c2b/v1/registerurl" \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "ShortCode": "174379",
    "ResponseType": "Completed",
    "ConfirmationURL": "https://your-domain.com/mpesa/confirm",
    "ValidationURL": "https://your-domain.com/mpesa/validate"
  }'

# 2. Simulate C2B payment
curl -X POST "https://sandbox.safaricom.co.ke/mpesa/c2b/v1/simulate" \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "ShortCode": "174379",
    "CommandID": "CustomerPayBillOnline",
    "Amount": "100",
    "Msisdn": "254708374149",
    "BillRefNumber": "kshs_1abc123xyz"
  }'
```

**What happens:**
1. Simulates user sending 100 KES to paybill 174379
2. Account reference: kshs_1abc123xyz (user's Kakitu address)
3. Your validation URL called (approve/reject)
4. Your confirmation URL called with transaction details
5. You mint 100 KSHS and send to kshs_1abc123xyz

### Flow 3: B2C Payment (Withdrawal Simulation)

This simulates sending KES to user's phone when they burn KSHS.

```bash
# Generate security credential (encrypted password)
# For sandbox, use: "Safcom496!"

curl -X POST "https://sandbox.safaricom.co.ke/mpesa/b2c/v1/paymentrequest" \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "InitiatorName": "testapi",
    "SecurityCredential": "ENCRYPTED_CREDENTIAL_HERE",
    "CommandID": "BusinessPayment",
    "Amount": "100",
    "PartyA": "174379",
    "PartyB": "254708374149",
    "Remarks": "KSHS Withdrawal",
    "QueueTimeOutURL": "https://your-domain.com/mpesa/b2c-timeout",
    "ResultURL": "https://your-domain.com/mpesa/b2c-result",
    "Occasion": "KSHS_REDEMPTION"
  }'
```

**What happens:**
1. System sends 100 KES to phone 254708374149
2. M-Pesa processes the payment
3. Result callback sent to your URL
4. User receives M-Pesa SMS confirmation

---

## 🌐 Webhook Testing with webhook.site

Since you need public URLs for callbacks, use webhook.site for testing:

### Setup:

1. Go to https://webhook.site
2. Copy your unique URL (e.g., `https://webhook.site/abc123`)
3. Use this URL for all callbacks:

```json
{
  "confirmation_url": "https://webhook.site/abc123/confirm",
  "validation_url": "https://webhook.site/abc123/validate",
  "stk_callback_url": "https://webhook.site/abc123/stk-callback",
  "b2c_result_url": "https://webhook.site/abc123/b2c-result"
}
```

4. Test payment flows and watch callbacks arrive in real-time
5. Inspect JSON payloads to build your integration

---

## 📊 Expected Callback Formats

### C2B Confirmation Callback

```json
{
  "TransactionType": "Pay Bill",
  "TransID": "QGK1234567",
  "TransTime": "20250122143000",
  "TransAmount": "100.00",
  "BusinessShortCode": "174379",
  "BillRefNumber": "kshs_1abc123xyz",
  "InvoiceNumber": "",
  "OrgAccountBalance": "50100.00",
  "ThirdPartyTransID": "",
  "MSISDN": "254708374149",
  "FirstName": "John",
  "MiddleName": "",
  "LastName": "Doe"
}
```

**Action:** Mint 100 KSHS and send to kshs_1abc123xyz

### STK Push Callback

```json
{
  "Body": {
    "stkCallback": {
      "MerchantRequestID": "29115-34620561-1",
      "CheckoutRequestID": "ws_CO_191220191020363925",
      "ResultCode": 0,
      "ResultDesc": "The service request is processed successfully.",
      "CallbackMetadata": {
        "Item": [
          {
            "Name": "Amount",
            "Value": 100
          },
          {
            "Name": "MpesaReceiptNumber",
            "Value": "QGK1234567"
          },
          {
            "Name": "Balance"
          },
          {
            "Name": "TransactionDate",
            "Value": 20250122143000
          },
          {
            "Name": "PhoneNumber",
            "Value": 254708374149
          }
        ]
      }
    }
  }
}
```

**Action:** Mint 100 KSHS and send to the account that initiated STK Push

### B2C Result Callback

```json
{
  "Result": {
    "ResultType": 0,
    "ResultCode": 0,
    "ResultDesc": "The service request is processed successfully.",
    "OriginatorConversationID": "10571-7910404-1",
    "ConversationID": "AG_20191219_00004e48cf7e3533f581",
    "TransactionID": "RFK1234567",
    "ResultParameters": {
      "ResultParameter": [
        {
          "Key": "TransactionAmount",
          "Value": 100
        },
        {
          "Key": "TransactionReceipt",
          "Value": "RFK1234567"
        },
        {
          "Key": "ReceiverPartyPublicName",
          "Value": "254708374149 - John Doe"
        },
        {
          "Key": "TransactionCompletedDateTime",
          "Value": "22.01.2025 14:30:00"
        },
        {
          "Key": "B2CUtilityAccountAvailableFunds",
          "Value": 49900.00
        }
      ]
    }
  }
}
```

**Action:** Mark withdrawal as complete, log with M-Pesa receipt RFK1234567

---

## 🧪 Testing Scenarios

### Scenario 1: Happy Path Deposit

```bash
# 1. User initiates deposit via STK Push
# 2. User enters PIN on phone
# 3. M-Pesa confirms payment
# 4. Your system receives callback
# 5. Mint 100 KSHS
# 6. Send to user's wallet
# 7. User sees balance updated

# Test with:
Amount: 100 KES
Phone: 254708374149
Account: kshs_1abc123xyz
```

### Scenario 2: Failed Deposit

```bash
# Simulate by canceling STK Push prompt
# Callback will have ResultCode != 0

# Your system should:
# 1. Log failed transaction
# 2. NOT mint any KSHS
# 3. Notify user of failure
```

### Scenario 3: Duplicate Transaction

```bash
# Send same transaction twice
# Your system should:
# 1. Check if TransID already processed
# 2. Reject duplicate
# 3. Log attempt
# 4. Return error
```

### Scenario 4: Happy Path Withdrawal

```bash
# 1. User sends 100 KSHS to burn address
# 2. Your system detects burn
# 3. Initiate B2C to send 100 KES
# 4. M-Pesa processes payment
# 5. User receives KES on phone
# 6. Log with both block hash and M-Pesa receipt
```

### Scenario 5: Insufficient Float

```bash
# If your M-Pesa account has < 100 KES
# B2C will fail with insufficient funds

# Your system should:
# 1. Alert admins
# 2. Queue withdrawal for retry
# 3. Notify user of delay
# 4. Auto-retry when float replenished
```

---

## 🔍 Debugging Tips

### Check OAuth Token

```bash
# Token expires after 1 hour
# If requests fail with 401, get new token

# Check token validity:
curl -X GET "https://sandbox.safaricom.co.ke/mpesa/accountbalance/v1/query" \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  ...

# If expired, you'll see:
# {"requestId":"xyz","errorCode":"401","errorMessage":"Invalid Access Token"}
```

### Check Transaction Status

```bash
# Query any transaction by ID
curl -X POST "https://sandbox.safaricom.co.ke/mpesa/transactionstatus/v1/query" \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "Initiator": "testapi",
    "SecurityCredential": "ENCRYPTED_CREDENTIAL",
    "CommandID": "TransactionStatusQuery",
    "TransactionID": "QGK1234567",
    "PartyA": "174379",
    "IdentifierType": "4",
    "ResultURL": "https://your-domain.com/result",
    "QueueTimeOutURL": "https://your-domain.com/timeout",
    "Remarks": "Status check",
    "Occasion": ""
  }'
```

### Common Error Codes

| Code | Meaning | Solution |
|------|---------|----------|
| 0 | Success | All good! |
| 1 | Insufficient Funds | User has insufficient balance |
| 17 | Wrong PIN | User entered wrong M-Pesa PIN |
| 26 | System Busy | Retry after a few seconds |
| 401 | Invalid Token | Get new OAuth token |
| 500 | Internal Error | Contact M-Pesa support |

---

## 📝 Integration Checklist

Before going live, test these scenarios:

- [ ] OAuth authentication works
- [ ] C2B URL registration succeeds
- [ ] STK Push initiates successfully
- [ ] STK Push callback received and parsed
- [ ] Duplicate transactions rejected
- [ ] Failed transactions handled gracefully
- [ ] B2C payment succeeds
- [ ] B2C callback received and parsed
- [ ] Account balance query works
- [ ] Transaction status query works
- [ ] Reserve reconciliation accurate
- [ ] All operations logged with proofs
- [ ] Webhook security implemented
- [ ] Rate limiting functional
- [ ] Error alerts working

---

## 🚀 Next Steps After Sandbox Testing

1. **Apply for Production Access**
   - Contact Safaricom: apisupport@safaricom.co.ke
   - Provide business registration
   - Get production shortcode and credentials

2. **Update Configuration**
   ```json
   {
     "environment": "production",
     "base_url": "https://api.safaricom.co.ke"
   }
   ```

3. **Security Hardening**
   - Use real HSM for mint authority key
   - Enable SSL for webhooks
   - Implement IP whitelisting
   - Set up monitoring/alerting

4. **Go Live!**
   - Start with limited user group
   - Monitor closely
   - Scale gradually

---

## 📞 Support

- **M-Pesa Sandbox Issues**: apisupport@safaricom.co.ke
- **Daraja Portal**: https://developer.safaricom.co.ke
- **Documentation**: https://developer.safaricom.co.ke/docs

---

## ⚠️ Important Notes

1. **Sandbox vs Production**
   - Sandbox uses test credentials (safe to share)
   - Production uses real credentials (NEVER share)
   - Sandbox transactions are simulated (no real money)
   - Production transactions are real (real money moves)

2. **Webhook URLs**
   - Sandbox: Can use localhost tunnels (ngrok, webhook.site)
   - Production: MUST use public HTTPS with valid SSL

3. **Rate Limits**
   - Sandbox: ~20 requests/second
   - Production: Higher limits, but still rate limited

4. **Support**
   - Sandbox: Limited support
   - Production: Full SLA support

---

Happy testing! 🎉
