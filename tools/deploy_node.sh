#!/bin/bash
#
# Kakitu Node Deployment Script
# Automated deployment for Kakitu M-Pesa integrated node operators
#
# Usage: sudo ./deploy_node.sh
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "Kakitu Node Deployment Script"
echo "========================================="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERROR: Please run as root (sudo)${NC}"
    exit 1
fi

# Check system requirements
echo "Checking system requirements..."

# Check CPU cores
CPU_CORES=$(nproc)
if [ "$CPU_CORES" -lt 2 ]; then
    echo -e "${YELLOW}WARNING: Less than 2 CPU cores detected. Performance may be limited.${NC}"
fi

# Check RAM
TOTAL_RAM=$(free -g | awk '/^Mem:/{print $2}')
if [ "$TOTAL_RAM" -lt 4 ]; then
    echo -e "${YELLOW}WARNING: Less than 4GB RAM detected. Recommended: 8GB+${NC}"
fi

# Check disk space
FREE_SPACE=$(df -BG / | awk 'NR==2 {print $4}' | sed 's/G//')
if [ "$FREE_SPACE" -lt 50 ]; then
    echo -e "${YELLOW}WARNING: Less than 50GB free space. Recommended: 100GB+${NC}"
fi

echo -e "${GREEN}✓ System requirements check complete${NC}"
echo ""

# Install dependencies
echo "Installing system dependencies..."
apt-get update -qq
apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    libjsoncpp-dev \
    pkg-config \
    curl \
    wget \
    jq \
    > /dev/null 2>&1

echo -e "${GREEN}✓ Dependencies installed${NC}"
echo ""

# Create kakitu user if doesn't exist
if ! id -u kakitu > /dev/null 2>&1; then
    echo "Creating kakitu system user..."
    useradd --system --home-dir /var/lib/kakitu --create-home --shell /bin/bash kakitu
    echo -e "${GREEN}✓ Kakitu user created${NC}"
else
    echo "Kakitu user already exists"
fi

# Create directories
echo "Creating directory structure..."
mkdir -p /var/lib/kakitu/{data,logs,config,backup}
mkdir -p /etc/kakitu
mkdir -p /usr/local/bin

# Copy binary
if [ -f "./nano_node" ]; then
    echo "Installing kakitu-node binary..."
    cp ./nano_node /usr/local/bin/kakitu-node
    chmod +x /usr/local/bin/kakitu-node
    echo -e "${GREEN}✓ Binary installed to /usr/local/bin/kakitu-node${NC}"
else
    echo -e "${RED}ERROR: nano_node binary not found. Please build first with 'make nano_node'${NC}"
    exit 1
fi

# Copy configuration
if [ -f "./kakitu_config.json" ]; then
    echo "Installing configuration..."
    cp ./kakitu_config.json /etc/kakitu/config.json
    echo -e "${GREEN}✓ Configuration installed${NC}"
else
    echo -e "${YELLOW}WARNING: kakitu_config.json not found. Please configure manually.${NC}"
fi

# Set permissions
chown -R kakitu:kakitu /var/lib/kakitu
chown -R kakitu:kakitu /etc/kakitu
chmod 700 /var/lib/kakitu/data
chmod 700 /etc/kakitu

echo -e "${GREEN}✓ Permissions configured${NC}"
echo ""

# Create systemd service
echo "Creating systemd service..."
cat > /etc/systemd/system/kakitu-node.service <<'EOF'
[Unit]
Description=Kakitu Node - M-Pesa Integrated Cryptocurrency Node
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=kakitu
Group=kakitu
WorkingDirectory=/var/lib/kakitu/data
ExecStart=/usr/local/bin/kakitu-node --daemon --data_path=/var/lib/kakitu/data --config_path=/etc/kakitu/config.json
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal
SyslogIdentifier=kakitu-node

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=full
ProtectHome=true
ReadWritePaths=/var/lib/kakitu

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096

# Environment
Environment="KAKITU_MINT_PRIVATE_KEY=REPLACE_WITH_YOUR_KEY"

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
echo -e "${GREEN}✓ Systemd service created${NC}"
echo ""

# Setup logrotate
echo "Configuring log rotation..."
cat > /etc/logrotate.d/kakitu-node <<'EOF'
/var/lib/kakitu/logs/*.log {
    daily
    rotate 14
    compress
    delaycompress
    notifempty
    create 0640 kakitu kakitu
    sharedscripts
    postrotate
        systemctl reload kakitu-node > /dev/null 2>&1 || true
    endscript
}
EOF

echo -e "${GREEN}✓ Log rotation configured${NC}"
echo ""

# Setup firewall (if ufw is available)
if command -v ufw > /dev/null 2>&1; then
    echo "Configuring firewall..."
    ufw allow 7075/tcp comment 'Kakitu Node P2P'
    ufw allow 7076/tcp comment 'Kakitu Node RPC'
    ufw allow 8080/tcp comment 'Kakitu M-Pesa Webhook'
    echo -e "${GREEN}✓ Firewall rules added${NC}"
fi

echo ""
echo "========================================="
echo "Installation Complete!"
echo "========================================="
echo ""
echo -e "${YELLOW}IMPORTANT: Before starting the node:${NC}"
echo ""
echo "1. Generate mint authority keypair:"
echo "   cd /home/user/kakitu-node/tools"
echo "   ./generate_mint_authority.sh"
echo ""
echo "2. Set the mint authority private key:"
echo "   Edit /etc/systemd/system/kakitu-node.service"
echo "   Replace REPLACE_WITH_YOUR_KEY with your actual key"
echo "   Run: systemctl daemon-reload"
echo ""
echo "3. Configure M-Pesa credentials:"
echo "   Edit /etc/kakitu/config.json"
echo "   Update webhook URLs to your domain"
echo ""
echo "4. Start the node:"
echo "   systemctl start kakitu-node"
echo "   systemctl enable kakitu-node  # Start on boot"
echo ""
echo "5. Check status:"
echo "   systemctl status kakitu-node"
echo "   journalctl -u kakitu-node -f"
echo ""
echo "========================================="
echo "Node Management Commands"
echo "========================================="
echo ""
echo "Start:    systemctl start kakitu-node"
echo "Stop:     systemctl stop kakitu-node"
echo "Restart:  systemctl restart kakitu-node"
echo "Status:   systemctl status kakitu-node"
echo "Logs:     journalctl -u kakitu-node -f"
echo ""
echo "Data dir: /var/lib/kakitu/data"
echo "Config:   /etc/kakitu/config.json"
echo "Logs:     /var/lib/kakitu/logs/"
echo ""
echo -e "${GREEN}Deployment complete!${NC}"
echo ""
