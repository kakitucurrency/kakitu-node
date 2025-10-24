#!/bin/bash
#
# Kakitu Mint Authority Key Generator
# Generates a secure mint authority keypair for the Kakitu node
#
# Usage: ./generate_mint_authority.sh
#

set -e

echo "========================================="
echo "Kakitu Mint Authority Key Generator"
echo "========================================="
echo ""

# Check if nano_node binary exists
if [ ! -f "../build/nano_node" ] && [ ! -f "../nano_node" ]; then
    echo "ERROR: nano_node binary not found"
    echo "Please build the node first with: make nano_node"
    exit 1
fi

# Find the binary
NANO_NODE=""
if [ -f "../build/nano_node" ]; then
    NANO_NODE="../build/nano_node"
elif [ -f "../nano_node" ]; then
    NANO_NODE="../nano_node"
fi

echo "Generating mint authority keypair..."
echo ""

# Generate a new keypair using the nano_node binary
# This generates a random seed and derives the keypair
KEY_OUTPUT=$(echo '{"action":"key_create"}' | $NANO_NODE --debug_rpc_call)

# Extract private and public keys
PRIVATE_KEY=$(echo "$KEY_OUTPUT" | grep -oP '(?<="private":")[^"]*' || true)
PUBLIC_KEY=$(echo "$KEY_OUTPUT" | grep -oP '(?<="public":")[^"]*' || true)
ACCOUNT=$(echo "$KEY_OUTPUT" | grep -oP '(?<="account":"kshs_)[^"]*' || true)

if [ -z "$PRIVATE_KEY" ]; then
    echo "ERROR: Failed to generate keypair"
    echo "Raw output: $KEY_OUTPUT"
    exit 1
fi

echo "✓ Mint Authority Keypair Generated"
echo ""
echo "========================================="
echo "PRIVATE KEY (KEEP SECRET!)"
echo "========================================="
echo "$PRIVATE_KEY"
echo ""
echo "========================================="
echo "PUBLIC KEY"
echo "========================================="
echo "$PUBLIC_KEY"
echo ""
echo "========================================="
echo "ACCOUNT ADDRESS"
echo "========================================="
echo "kshs_$ACCOUNT"
echo ""
echo "========================================="
echo "SETUP INSTRUCTIONS"
echo "========================================="
echo ""
echo "1. Save the private key securely (password manager, hardware wallet, etc.)"
echo ""
echo "2. Set environment variable:"
echo "   export KAKITU_MINT_PRIVATE_KEY=\"$PRIVATE_KEY\""
echo ""
echo "3. Add to ~/.bashrc or ~/.profile for persistence:"
echo "   echo 'export KAKITU_MINT_PRIVATE_KEY=\"$PRIVATE_KEY\"' >> ~/.bashrc"
echo ""
echo "4. For systemd service, add to /etc/systemd/system/kakitu-node.service:"
echo "   Environment=\"KAKITU_MINT_PRIVATE_KEY=$PRIVATE_KEY\""
echo ""
echo "5. The mint authority account is: kshs_$ACCOUNT"
echo "   This account must be funded with a small amount for transaction fees"
echo ""
echo "========================================="
echo "SECURITY WARNING"
echo "========================================="
echo ""
echo "⚠️  NEVER share your private key with anyone"
echo "⚠️  NEVER commit it to version control"
echo "⚠️  NEVER send it over insecure channels"
echo "⚠️  Store it in a secure location with backups"
echo ""
echo "The mint authority can create KSHS tokens backed by M-Pesa deposits."
echo "Compromise of this key would allow unauthorized token creation."
echo ""

# Optionally save to secure file
read -p "Save keys to encrypted file? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    KEY_FILE="mint_authority_$(date +%Y%m%d_%H%M%S).key"

    echo "Private Key: $PRIVATE_KEY" > "$KEY_FILE"
    echo "Public Key: $PUBLIC_KEY" >> "$KEY_FILE"
    echo "Account: kshs_$ACCOUNT" >> "$KEY_FILE"
    echo "Generated: $(date)" >> "$KEY_FILE"

    chmod 600 "$KEY_FILE"

    echo ""
    echo "✓ Keys saved to: $KEY_FILE"
    echo "✓ File permissions set to 600 (owner read/write only)"
    echo ""
    echo "Consider encrypting this file with:"
    echo "  gpg -c $KEY_FILE"
    echo "  rm $KEY_FILE"
fi

echo ""
echo "Generation complete!"
echo ""
