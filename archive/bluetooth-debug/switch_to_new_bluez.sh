#!/bin/bash
# Switch systemd to use the new BlueZ 5.71 binary

echo "Switching to BlueZ 5.71..."
echo ""

# Update enable-audio.conf to use new path
cat > /etc/systemd/system/bluetooth.service.d/enable-audio.conf <<'EOF'
[Service]
# Override NVIDIA config - enable audio plugins and experimental features for HeadUnit
ExecStart=
ExecStart=/usr/libexec/bluetooth/bluetoothd -d --experimental
EOF

echo "✓ Updated enable-audio.conf"
echo ""

# Reload and restart
systemctl daemon-reload
systemctl restart bluetooth

echo "✓ Bluetooth restarted"
echo ""

# Wait for service to start
sleep 2

echo "=== New BlueZ Status ==="
echo "Version: $(/usr/libexec/bluetooth/bluetoothd --version)"
echo ""
ps aux | grep bluetoothd | grep -v grep
echo ""
echo "✓ BlueZ 5.71 is now active!"
