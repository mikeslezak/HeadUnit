#!/bin/bash
# Update BlueZ to enable both debug and experimental features

echo "Updating BlueZ configuration..."

# Update the enable-audio.conf to include experimental flag
cat > /etc/systemd/system/bluetooth.service.d/enable-audio.conf <<'EOF'
[Service]
# Override NVIDIA config - enable audio plugins and experimental features for HeadUnit
ExecStart=
ExecStart=/usr/lib/bluetooth/bluetoothd -d --experimental
EOF

echo "Configuration updated. Restarting Bluetooth service..."

# Reload systemd and restart Bluetooth
systemctl daemon-reload
systemctl restart bluetooth

echo ""
echo "✓ BlueZ updated with experimental features enabled!"
echo ""
echo "Now disconnect and reconnect your iPhone to Bluetooth."
echo "The battery percentage should then display correctly in the HeadUnit."
