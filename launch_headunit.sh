#!/bin/bash
# HeadUnit Launch Script
# This prevents Qt from inheriting GNOME scaling settings

# Disable Qt scaling - use exact pixel dimensions
export QT_AUTO_SCREEN_SCALE_FACTOR=0
export QT_SCALE_FACTOR=1
export QT_FONT_DPI=96

# Force Qt to respect the rotated screen geometry
export QT_QPA_PLATFORM=xcb

# Set DISPLAY
export DISPLAY=:0

# Apply touchscreen calibration for left-rotated 1560x720 display
# Find the Waveshare touchscreen device and apply calibration matrix
TOUCH_DEVICE_ID=$(xinput list | grep "Waveshare.*Waveshare" | grep -oP 'id=\K\d+')
if [ -n "$TOUCH_DEVICE_ID" ]; then
    xinput set-prop "$TOUCH_DEVICE_ID" "Coordinate Transformation Matrix" 0 -1 1 1 0 0 0 0 1
fi

# Load API keys
set -a
source /home/mike/HeadUnit/.env
set +a

# Launch HeadUnit
cd /home/mike/HeadUnit/build
./appHeadUnit
