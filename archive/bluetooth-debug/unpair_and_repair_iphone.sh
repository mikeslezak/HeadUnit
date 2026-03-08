#!/bin/bash
# Unpair and re-pair iPhone to enable Battery1 interface

echo "Unpairing iPhone..."
bluetoothctl remove 80:96:98:C8:69:17

echo ""
echo "iPhone has been unpaired."
echo ""
echo "Now follow these steps to re-pair:"
echo "1. On your iPhone, go to Settings → Bluetooth"
echo "2. Tap the (i) icon next to 'mike-HeadUnit' or the HeadUnit name"
echo "3. Tap 'Forget This Device' and confirm"
echo "4. Wait a few seconds"
echo "5. Your iPhone should appear in the available devices list on the HeadUnit"
echo "6. Pair with it again"
echo ""
echo "After re-pairing with experimental features enabled, the Battery1 interface"
echo "should be available and battery percentage will display correctly!"
