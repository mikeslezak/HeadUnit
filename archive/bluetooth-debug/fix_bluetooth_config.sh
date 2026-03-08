#!/bin/bash
# Fix Bluetooth configuration - remove conflicting files and restart properly

echo "Fixing BlueZ configuration..."

# Remove the duplicate experimental.conf from first failed attempt
rm -f /etc/systemd/system/bluetooth.service.d/experimental.conf

# Verify enable-audio.conf has correct configuration
cat > /etc/systemd/system/bluetooth.service.d/enable-audio.conf <<'EOF'
[Service]
# Override NVIDIA config - enable audio plugins and experimental features for HeadUnit
ExecStart=
ExecStart=/usr/lib/bluetooth/bluetoothd -d --experimental
EOF

echo "Configuration files updated."
echo ""

# Reload systemd configuration
echo "Reloading systemd daemon..."
systemctl daemon-reload

echo ""
echo "Restarting Bluetooth service..."
systemctl restart bluetooth

echo ""
echo "Waiting for service to start..."
sleep 2

echo ""
echo "=== Bluetooth Service Status ==="
systemctl status bluetooth.service --no-pager | head -15

echo ""
echo "=== Bluetooth Process Arguments ==="
ps aux | grep bluetoothd | grep -v grep

echo ""
echo "✓ Configuration applied!"
echo ""
echo "Next steps:"
echo "1. Disconnect your iPhone from Bluetooth"
echo "2. Reconnect your iPhone to Bluetooth"
echo "3. Relaunch the HeadUnit app"
