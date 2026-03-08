#!/bin/bash
# Enable BlueZ experimental features for iPhone battery reporting

echo "Enabling BlueZ experimental features for battery reporting..."

# Create systemd override directory
mkdir -p /etc/systemd/system/bluetooth.service.d

# Create override configuration
cat > /etc/systemd/system/bluetooth.service.d/experimental.conf <<'EOF'
[Service]
ExecStart=
ExecStart=/usr/lib/bluetooth/bluetoothd --experimental
EOF

echo "Configuration created. Restarting Bluetooth service..."

# Reload systemd and restart Bluetooth
systemctl daemon-reload
systemctl restart bluetooth

echo ""
echo "✓ BlueZ experimental features enabled!"
echo ""
echo "Now disconnect and reconnect your iPhone to Bluetooth."
echo "The battery percentage should then display correctly in the HeadUnit."
