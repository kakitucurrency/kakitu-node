# Kakitu Node Operator Guide

## Welcome to Kakitu Network

Thank you for becoming a Kakitu node operator! This guide will help you set up, secure, and operate your node to earn rewards while supporting Kenya's first M-Pesa-backed cryptocurrency.

---

## Table of Contents

1. [Overview](#overview)
2. [System Requirements](#system-requirements)
3. [Installation](#installation)
4. [Configuration](#configuration)
5. [Security Best Practices](#security-best-practices)
6. [Operations & Maintenance](#operations--maintenance)
7. [Monitoring & Troubleshooting](#monitoring--troubleshooting)
8. [Revenue Sharing](#revenue-sharing)
9. [Support](#support)

---

## Overview

### What is a Kakitu Node?

A Kakitu node is a server that:
- Validates and processes KSHS transactions
- Integrates with M-Pesa for deposits and withdrawals
- Maintains the Kakitu blockchain ledger
- Earns revenue from transaction fees (50% to node operators)

### Node Types

1. **Principal Nodes** (Geographic distribution bonuses)
   - One per county in Kenya
   - Higher revenue share
   - Requires reliable uptime

2. **Standard Nodes** (General network support)
   - Distributed globally
   - Standard revenue share
   - Essential for network resilience

3. **Community Nodes** (Local access points)
   - Serve specific communities
   - Educational role
   - Lower barrier to entry

---

## System Requirements

### Minimum Requirements
- **CPU**: 2 cores (4+ recommended)
- **RAM**: 4GB (8GB+ recommended)
- **Storage**: 50GB SSD (100GB+ recommended)
- **Network**: 10 Mbps up/down, unlimited data
- **OS**: Ubuntu 20.04 LTS or later

### Recommended Hardware
- **CPU**: 4-8 cores
- **RAM**: 16GB
- **Storage**: 256GB NVMe SSD
- **Network**: 100 Mbps fiber connection
- **Backup Power**: UPS (4+ hours)

### Network Requirements
- Static IP address (recommended)
- Domain name with SSL certificate
- Open ports: 7075 (P2P), 7076 (RPC), 8080 (Webhook)

---

## Installation

### Step 1: Prepare Your Server

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install dependencies
sudo apt install -y build-essential cmake git \
    libboost-all-dev libcurl4-openssl-dev libssl-dev \
    libjsoncpp-dev pkg-config curl wget jq

# Create kakitu user
sudo useradd --system --home-dir /var/lib/kakitu \
    --create-home --shell /bin/bash kakitu
```

### Step 2: Build from Source

```bash
# Clone repository
git clone https://github.com/kakitucurrency/kakitu-node.git
cd kakitu-node

# Checkout stable branch
git checkout main

# Build
cmake . -DACTIVE_NETWORK=kshs_live_network
make nano_node -j$(nproc)
```

### Step 3: Deploy Node

```bash
# Run deployment script
sudo tools/deploy_node.sh
```

This script:
- Installs the binary to `/usr/local/bin/kakitu-node`
- Creates configuration directory `/etc/kakitu/`
- Sets up systemd service
- Configures log rotation
- Sets firewall rules

### Step 4: Generate Mint Authority Keys

```bash
cd tools
./generate_mint_authority.sh
```

**IMPORTANT**: Save your private key securely! You'll need it for node configuration.

---

## Configuration

### 1. Edit Configuration File

```bash
sudo nano /etc/kakitu/config.json
```

Key settings to update:

#### M-Pesa Credentials
```json
"credentials": {
  "consumer_key": "YOUR_CONSUMER_KEY",
  "consumer_secret": "YOUR_CONSUMER_SECRET",
  "shortcode": "YOUR_SHORTCODE",
  "passkey": "YOUR_PASSKEY"
}
```

#### Webhook URLs (use your domain)
```json
"webhook_urls": {
  "stk_callback_url": "https://your-domain.com/mpesa/stk-callback",
  "b2c_result_url": "https://your-domain.com/mpesa/b2c-result",
  "b2c_timeout_url": "https://your-domain.com/mpesa/b2c-timeout"
}
```

### 2. Set Mint Authority Key

```bash
sudo nano /etc/systemd/system/kakitu-node.service
```

Replace `REPLACE_WITH_YOUR_KEY` with your actual mint private key:

```ini
Environment="KAKITU_MINT_PRIVATE_KEY=YOUR_PRIVATE_KEY_HERE"
```

Reload systemd:
```bash
sudo systemctl daemon-reload
```

### 3. Configure SSL/TLS (for webhooks)

```bash
# Install certbot
sudo apt install -y certbot

# Get certificate
sudo certbot certonly --standalone -d your-domain.com

# Configure nginx as reverse proxy
sudo apt install -y nginx
```

Example nginx config (`/etc/nginx/sites-available/kakitu-webhook`):

```nginx
server {
    listen 443 ssl;
    server_name your-domain.com;

    ssl_certificate /etc/letsencrypt/live/your-domain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/your-domain.com/privkey.pem;

    location /mpesa/ {
        proxy_pass http://localhost:8080/mpesa/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

Enable and start:
```bash
sudo ln -s /etc/nginx/sites-available/kakitu-webhook /etc/nginx/sites-enabled/
sudo systemctl restart nginx
```

---

## Security Best Practices

### 1. Secure Your Private Keys

- **Never** share your mint authority private key
- Store backup on encrypted offline storage
- Consider hardware security module (HSM) for production
- Use environment variables, never hardcode in files

### 2. Firewall Configuration

```bash
# Enable UFW
sudo ufw enable

# Allow only necessary ports
sudo ufw allow 22/tcp    # SSH (restrict to your IP)
sudo ufw allow 7075/tcp  # Kakitu P2P
sudo ufw allow 7076/tcp  # Kakitu RPC (restrict if not public)
sudo ufw allow 8080/tcp  # M-Pesa webhooks
sudo ufw allow 443/tcp   # HTTPS
```

### 3. SSH Hardening

```bash
# Disable password authentication
sudo nano /etc/ssh/sshd_config
```

Set:
```
PasswordAuthentication no
PermitRootLogin no
```

Restart SSH:
```bash
sudo systemctl restart sshd
```

### 4. Regular Updates

```bash
# Weekly security updates
sudo apt update && sudo apt upgrade -y
```

### 5. Monitoring & Alerts

Set up monitoring for:
- Node uptime
- Disk space usage
- Memory usage
- Failed webhook attempts
- Unusual transaction patterns

---

## Operations & Maintenance

### Starting the Node

```bash
# Start node
sudo systemctl start kakitu-node

# Enable on boot
sudo systemctl enable kakitu-node

# Check status
sudo systemctl status kakitu-node
```

### Viewing Logs

```bash
# Real-time logs
sudo journalctl -u kakitu-node -f

# Last 100 lines
sudo journalctl -u kakitu-node -n 100

# Search for errors
sudo journalctl -u kakitu-node | grep ERROR
```

### Common Operations

#### Check Node Sync Status
```bash
curl -X POST http://localhost:7076 -d '{
  "action": "block_count"
}'
```

#### Check Supply Info
```bash
curl -X POST http://localhost:7076 -d '{
  "action": "supply_info"
}'
```

#### View Operation History
```bash
curl -X POST http://localhost:7076 -d '{
  "action": "operation_history",
  "count": "10"
}'
```

### Backup Procedures

#### Daily Backup Script
```bash
#!/bin/bash
BACKUP_DIR="/var/backups/kakitu"
DATE=$(date +%Y%m%d_%H%M%S)

mkdir -p $BACKUP_DIR

# Backup data directory
tar -czf $BACKUP_DIR/data_$DATE.tar.gz /var/lib/kakitu/data/

# Backup configuration
cp /etc/kakitu/config.json $BACKUP_DIR/config_$DATE.json

# Keep only last 7 days
find $BACKUP_DIR -type f -mtime +7 -delete
```

### Updating the Node

```bash
# Stop node
sudo systemctl stop kakitu-node

# Backup first!
sudo tools/backup.sh

# Pull latest code
cd /home/user/kakitu-node
git pull origin main

# Rebuild
make nano_node -j$(nproc)

# Redeploy
sudo tools/deploy_node.sh

# Start node
sudo systemctl start kakitu-node

# Check status
sudo systemctl status kakitu-node
```

---

## Monitoring & Troubleshooting

### Health Checks

Create `/etc/cron.d/kakitu-health`:

```bash
*/5 * * * * root /usr/local/bin/kakitu-health-check.sh
```

Health check script (`/usr/local/bin/kakitu-health-check.sh`):

```bash
#!/bin/bash

# Check if node is running
if ! systemctl is-active --quiet kakitu-node; then
    echo "ALERT: Kakitu node is down!" | mail -s "Node Alert" admin@yourdomain.com
    systemctl start kakitu-node
fi

# Check disk space
DISK_USAGE=$(df -h / | awk 'NR==2 {print $5}' | sed 's/%//')
if [ $DISK_USAGE -gt 85 ]; then
    echo "ALERT: Disk usage at ${DISK_USAGE}%" | mail -s "Disk Alert" admin@yourdomain.com
fi

# Check if node is synced
BLOCK_COUNT=$(curl -s -X POST http://localhost:7076 -d '{"action":"block_count"}' | jq -r '.count')
if [ -z "$BLOCK_COUNT" ]; then
    echo "ALERT: Node not responding to RPC" | mail -s "RPC Alert" admin@yourdomain.com
fi
```

### Common Issues

#### Node Won't Start

1. Check logs: `sudo journalctl -u kakitu-node -n 100`
2. Verify configuration: `cat /etc/kakitu/config.json | jq .`
3. Check permissions: `ls -la /var/lib/kakitu/`
4. Verify binary: `/usr/local/bin/kakitu-node --version`

#### Webhook Connection Failures

1. Test M-Pesa connectivity: `curl https://sandbox.safaricom.co.ke`
2. Check credentials in config
3. Verify webhook server is running: `netstat -tlnp | grep 8080`
4. Check SSL certificate: `certbot certificates`

#### Sync Issues

1. Check network connectivity
2. Verify peers: Check P2P connections in logs
3. Clear database and resync:
   ```bash
   sudo systemctl stop kakitu-node
   sudo rm -rf /var/lib/kakitu/data/data.ldb*
   sudo systemctl start kakitu-node
   ```

#### High Memory Usage

1. Check for memory leaks: `ps aux | grep kakitu-node`
2. Restart node: `sudo systemctl restart kakitu-node`
3. Consider upgrading RAM if persistent

---

## Revenue Sharing

### How Node Operators Earn

Kakitu implements a fair revenue sharing model:

- **50% of transaction fees** distributed to node operators
- Revenue weighted by:
  - Voting weight (stake)
  - Uptime (reliability)
  - Geographic location (bonuses for Principal nodes)

### Payment Schedule

- Distributions every 10,000 blocks (~1 week)
- Minimum payout: 100 KSHS
- Automatically sent to your node's representative account

### Maximizing Revenue

1. **Maintain high uptime** (99.5%+)
2. **Increase voting weight** (hold KSHS in node account)
3. **Become a Principal Node** (one per county)
4. **Process webhooks quickly** (optimize server performance)
5. **Provide RPC services** (enable public RPC if bandwidth allows)

### Checking Your Earnings

```bash
curl -X POST http://localhost:7076 -d '{
  "action": "account_balance",
  "account": "YOUR_NODE_ACCOUNT"
}'
```

---

## Support

### Resources

- **Documentation**: https://docs.kakitu.com
- **GitHub**: https://github.com/kakitucurrency/kakitu-node
- **Telegram**: https://t.me/kakitu_operators
- **Discord**: https://discord.gg/kakitu

### Reporting Issues

1. Check logs: `sudo journalctl -u kakitu-node -n 500 > /tmp/kakitu-logs.txt`
2. Check configuration: `cat /etc/kakitu/config.json | jq .`
3. System info: `uname -a && free -h && df -h`
4. Open issue: https://github.com/kakitucurrency/kakitu-node/issues

### Community Support

Join our operator community:
- Monthly operator calls
- Shared troubleshooting knowledge base
- Network governance participation
- Feature requests and voting

---

## Appendix

### Configuration Reference

See `kakitu_config.json` for full configuration options.

### RPC API Reference

Full API documentation: https://docs.kakitu.com/rpc-api

### Network Governance

Node operators participate in:
- Protocol upgrade votes
- Fee adjustment proposals
- Network parameter changes
- Grant funding decisions

---

## License

Kakitu Node Software is licensed under BSD 3-Clause License.

---

**Thank you for supporting the Kakitu network!**

For questions or support, contact: operators@kakitu.com
