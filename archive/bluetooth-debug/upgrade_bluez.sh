#!/bin/bash
# Upgrade BlueZ to newer version for better iPhone battery support
# This script will download, compile, and install BlueZ 5.71

set -e  # Exit on error

BLUEZ_VERSION="5.71"
BUILD_DIR="/tmp/bluez-build"

echo "========================================"
echo "BlueZ Upgrade Script"
echo "Current version: $(bluetoothd --version)"
echo "Target version: $BLUEZ_VERSION"
echo "========================================"
echo ""

# Backup current configuration
echo "1. Backing up current configuration..."
sudo cp -r /etc/bluetooth /etc/bluetooth.backup
sudo cp -r /etc/systemd/system/bluetooth.service.d /etc/systemd/system/bluetooth.service.d.backup
echo "  ✓ Backup created"
echo ""

# Install build dependencies
echo "2. Installing build dependencies..."
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    libdbus-1-dev \
    libglib2.0-dev \
    libical-dev \
    libreadline-dev \
    libudev-dev \
    libusb-dev \
    python3-docutils \
    libjson-c-dev \
    libelf-dev
echo "  ✓ Dependencies installed"
echo ""

# Download BlueZ
echo "3. Downloading BlueZ $BLUEZ_VERSION..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
wget -q --show-progress "http://www.kernel.org/pub/linux/bluetooth/bluez-${BLUEZ_VERSION}.tar.xz"
tar xf "bluez-${BLUEZ_VERSION}.tar.xz"
cd "bluez-${BLUEZ_VERSION}"
echo "  ✓ Downloaded and extracted"
echo ""

# Configure and build
echo "4. Configuring BlueZ..."
./configure \
    --prefix=/usr \
    --mandir=/usr/share/man \
    --sysconfdir=/etc \
    --localstatedir=/var \
    --enable-library \
    --enable-experimental \
    --enable-maintainer-mode \
    --enable-deprecated
echo "  ✓ Configuration complete"
echo ""

echo "5. Building BlueZ (this may take 10-15 minutes)..."
make -j$(nproc)
echo "  ✓ Build complete"
echo ""

# Stop Bluetooth service
echo "6. Stopping Bluetooth service..."
sudo systemctl stop bluetooth
echo "  ✓ Service stopped"
echo ""

# Install
echo "7. Installing BlueZ..."
sudo make install
echo "  ✓ Installation complete"
echo ""

# Restore configuration
echo "8. Restoring configuration..."
sudo cp -r /etc/bluetooth.backup/* /etc/bluetooth/
sudo cp -r /etc/systemd/system/bluetooth.service.d.backup/* /etc/systemd/system/bluetooth.service.d/
echo "  ✓ Configuration restored"
echo ""

# Reload and restart
echo "9. Reloading systemd and starting Bluetooth..."
sudo systemctl daemon-reload
sudo systemctl start bluetooth
sleep 2
echo "  ✓ Bluetooth restarted"
echo ""

echo "========================================"
echo "Upgrade Complete!"
echo "New version: $(bluetoothd --version)"
echo "========================================"
echo ""
echo "Next steps:"
echo "1. Verify Bluetooth is running: systemctl status bluetooth"
echo "2. Check process: ps aux | grep bluetoothd"
echo "3. Re-pair your iPhone"
echo "4. Relaunch HeadUnit app"
echo ""
echo "If something went wrong, you can restore from backup:"
echo "  sudo cp -r /etc/bluetooth.backup/* /etc/bluetooth/"
echo "  sudo apt-get install --reinstall bluez"
