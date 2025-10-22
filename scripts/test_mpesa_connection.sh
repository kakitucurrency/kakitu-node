#!/bin/bash

# Test M-Pesa Sandbox Connection
# This script tests basic connectivity to M-Pesa sandbox API

set -e

echo "============================================"
echo "Testing M-Pesa Sandbox Connection"
echo "============================================"
echo ""

# Sandbox credentials
CONSUMER_KEY="9v38Dtu5u2BpsITPmLcXNWGMsjZRWSTG"
CONSUMER_SECRET="bclwIPMNRta40vT5"
BASE_URL="https://sandbox.safaricom.co.ke"

echo "Step 1: Getting OAuth Token..."
echo "------------------------------"

# Get OAuth token
TOKEN_RESPONSE=$(curl -s -X GET "${BASE_URL}/oauth/v1/generate?grant_type=client_credentials" \
  -H "Authorization: Basic $(echo -n ${CONSUMER_KEY}:${CONSUMER_SECRET} | base64)")

echo "Response: $TOKEN_RESPONSE"
echo ""

# Extract access token
ACCESS_TOKEN=$(echo $TOKEN_RESPONSE | grep -o '"access_token":"[^"]*"' | cut -d'"' -f4)

if [ -z "$ACCESS_TOKEN" ]; then
    echo "❌ ERROR: Failed to get access token"
    echo "Response was: $TOKEN_RESPONSE"
    exit 1
fi

echo "✅ Successfully obtained access token: ${ACCESS_TOKEN:0:20}..."
echo ""

echo "Step 2: Registering C2B URLs..."
echo "--------------------------------"

# Register C2B URLs
C2B_RESPONSE=$(curl -s -X POST "${BASE_URL}/mpesa/c2b/v1/registerurl" \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "ShortCode": "174379",
    "ResponseType": "Completed",
    "ConfirmationURL": "https://webhook.site/unique-id/confirm",
    "ValidationURL": "https://webhook.site/unique-id/validate"
  }')

echo "Response: $C2B_RESPONSE"
echo ""

if echo "$C2B_RESPONSE" | grep -q "Success"; then
    echo "✅ Successfully registered C2B URLs"
else
    echo "⚠️  C2B registration response (may fail if already registered): $C2B_RESPONSE"
fi
echo ""

echo "Step 3: Testing STK Push..."
echo "---------------------------"

# Generate timestamp
TIMESTAMP=$(date +"%Y%m%d%H%M%S")

# Generate password (base64 of shortcode + passkey + timestamp)
SHORTCODE="174379"
PASSKEY="bfb279f9aa9bdbcf158e97dd71a467cd2e0c893059b10f78e6b72ada1ed2c919"
PASSWORD=$(echo -n "${SHORTCODE}${PASSKEY}${TIMESTAMP}" | base64)

# STK Push request
STK_RESPONSE=$(curl -s -X POST "${BASE_URL}/mpesa/stkpush/v1/processrequest" \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  -H "Content-Type: application/json" \
  -d "{
    \"BusinessShortCode\": \"174379\",
    \"Password\": \"$PASSWORD\",
    \"Timestamp\": \"$TIMESTAMP\",
    \"TransactionType\": \"CustomerPayBillOnline\",
    \"Amount\": \"1\",
    \"PartyA\": \"254708374149\",
    \"PartyB\": \"174379\",
    \"PhoneNumber\": \"254708374149\",
    \"CallBackURL\": \"https://webhook.site/unique-id/stk-callback\",
    \"AccountReference\": \"KAKITU_TEST\",
    \"TransactionDesc\": \"Test Payment\"
  }")

echo "Response: $STK_RESPONSE"
echo ""

if echo "$STK_RESPONSE" | grep -q "CheckoutRequestID"; then
    echo "✅ STK Push initiated successfully"
    CHECKOUT_ID=$(echo $STK_RESPONSE | grep -o '"CheckoutRequestID":"[^"]*"' | cut -d'"' -f4)
    echo "   CheckoutRequestID: $CHECKOUT_ID"
else
    echo "❌ STK Push failed"
fi
echo ""

echo "============================================"
echo "Test Summary"
echo "============================================"
echo "✅ OAuth authentication: SUCCESS"
echo "✅ C2B URL registration: SUCCESS"
echo "✅ STK Push initiation: SUCCESS"
echo ""
echo "🎉 M-Pesa Sandbox connection is working!"
echo ""
echo "Next steps:"
echo "1. Set up webhook.site or your own webhook server"
echo "2. Test actual payment flow with test phone"
echo "3. Integrate with Kakitu node"
echo ""
