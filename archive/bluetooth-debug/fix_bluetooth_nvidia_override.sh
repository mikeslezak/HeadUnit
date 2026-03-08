#!/bin/bash
# Fix Bluetooth configuration - mask NVIDIA override that's blocking experimental features

echo "Masking NVIDIA Bluetooth configuration..."

# Create a symlink to /dev/null to mask the NVIDIA config
mkdir -p /etc/systemd/system/bluetooth.service.d
ln -sf /dev/null /etc/systemd/system/bluetooth.service.d/nv-bluetooth-service.conf

echo "NVIDIA config masked."
echo ""

# Ensure our enable-audio.conf has the correct configuration
cat > /etc/systemd/system/bluetooth.service.d/enable-audio.conf <<'EOF'
[Service]
# Override NVIDIA config - enable audio plugins and experimental features for HeadUnit
ExecStart=
ExecStart=/usr/lib/bluetooth/bluetoothd -d --experimental
EOF

echo "Configuration updated."
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
echo "=== Bluetooth Service Drop-Ins ==="
systemctl cat bluetooth.service | grep -A 2 "# /etc/systemd/system"

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
