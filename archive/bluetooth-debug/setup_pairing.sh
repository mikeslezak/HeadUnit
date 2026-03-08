#!/bin/bash
# Setup Bluetooth for pairing

echo "Setting up Bluetooth for pairing..."
echo ""

# Enter bluetoothctl and run commands
bluetoothctl << EOF
power on
pairable on
discoverable on
agent on
default-agent
remove 80:96:98:C8:69:17
scan on
EOF

echo ""
echo "✓ Bluetooth is now ready for pairing!"
echo ""
echo "The HeadUnit is now:"
echo "  - Powered on"
echo "  - Pairable"
echo "  - Discoverable as 'mike-HeadUnit'"
echo "  - Scanning for devices"
echo ""
echo "On your iPhone:"
echo "  1. Go to Settings → Bluetooth"
echo "  2. Make sure Bluetooth is ON"
echo "  3. You should see 'mike-HeadUnit' in the device list"
echo "  4. Tap on 'mike-HeadUnit' to pair"
echo "  5. Accept any pairing requests"
echo ""
echo "Leave this terminal open - you'll see pairing requests here."
echo "Press Ctrl+C when pairing is complete."
