#!/bin/bash
# HeadUnit Launch Script
# This prevents Qt from inheriting GNOME scaling settings

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Disable Qt scaling - use exact pixel dimensions
export QT_AUTO_SCREEN_SCALE_FACTOR=0
export QT_SCALE_FACTOR=1
export QT_FONT_DPI=96

# Force Qt to respect the rotated screen geometry
export QT_QPA_PLATFORM=xcb

# Note: QSG_RENDER_LOOP=basic is incompatible with Tegra (EGL_BAD_CONTEXT).
# The threaded loop (default) works correctly with MapLibre OpenGL rendering.

# Set DISPLAY
export DISPLAY=:0

# Apply touchscreen calibration for left-rotated 1560x720 display
# Find the Waveshare touchscreen device and apply calibration matrix
if command -v xinput &>/dev/null; then
    TOUCH_DEVICE_ID=$(xinput list | grep "Waveshare.*Waveshare" | grep -oP 'id=\K\d+')
    if [ -n "$TOUCH_DEVICE_ID" ]; then
        xinput set-prop "$TOUCH_DEVICE_ID" "Coordinate Transformation Matrix" 0 -1 1 1 0 0 0 0 1
    fi
fi

# MapLibre Native Qt runtime paths
MAPLIBRE_DIR="$SCRIPT_DIR/external/maplibre-install"
export LD_LIBRARY_PATH="${MAPLIBRE_DIR}/lib:${LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${MAPLIBRE_DIR}/plugins:${QT_PLUGIN_PATH}"
export QML_IMPORT_PATH="${MAPLIBRE_DIR}/qml:${QML_IMPORT_PATH}"
export QSG_RHI_BACKEND=opengl

# Load API keys
if [ -f "$SCRIPT_DIR/.env" ]; then
    set -a
    source "$SCRIPT_DIR/.env"
    set +a
else
    echo "WARNING: $SCRIPT_DIR/.env not found - API keys will not be loaded"
fi

# MapLibre reads this env var for API key (used for Mapbox tile requests)
export MLN_API_KEY="${MAPBOX_TOKEN}"

# Start Tidal service (background, auto-restart)
TIDAL_SERVICE="$SCRIPT_DIR/services/tidal_service.py"
if [ -f "$TIDAL_SERVICE" ] && command -v python3 &>/dev/null; then
    # Kill any existing tidal service
    pkill -f "tidal_service.py" 2>/dev/null
    rm -f /tmp/headunit_tidal.sock
    python3 "$TIDAL_SERVICE" &
    TIDAL_PID=$!
    echo "Tidal service started (PID: $TIDAL_PID)"
fi

# Start Spotify service (background, auto-restart)
SPOTIFY_SERVICE="$SCRIPT_DIR/services/spotify_service.py"
if [ -f "$SPOTIFY_SERVICE" ] && command -v python3 &>/dev/null; then
    # Kill any existing spotify service
    pkill -f "spotify_service.py" 2>/dev/null
    rm -f /tmp/headunit_spotify.sock
    python3 "$SPOTIFY_SERVICE" &
    SPOTIFY_PID=$!
    echo "Spotify service started (PID: $SPOTIFY_PID)"
fi

# Launch HeadUnit (auto-restart on clean exit for OTA updates)
if [ ! -f "$SCRIPT_DIR/build/appHeadUnit" ]; then
    echo "ERROR: $SCRIPT_DIR/build/appHeadUnit not found - run cmake build first"
    exit 1
fi

cd "$SCRIPT_DIR/build"

MAX_RESTARTS=3
RESTART_COUNT=0

while [ $RESTART_COUNT -lt $MAX_RESTARTS ]; do
    ./appHeadUnit
    EXIT_CODE=$?

    if [ $EXIT_CODE -ne 0 ]; then
        # Non-zero exit = crash or signal, don't restart
        echo "HeadUnit exited with code $EXIT_CODE"
        break
    fi

    # Clean exit (code 0) = restart requested (e.g., after OTA update)
    RESTART_COUNT=$((RESTART_COUNT + 1))
    echo "HeadUnit requested restart ($RESTART_COUNT/$MAX_RESTARTS)..."
    sleep 1
done

# Cleanup: stop Tidal and Spotify services when HeadUnit exits
if [ -n "$TIDAL_PID" ]; then
    kill "$TIDAL_PID" 2>/dev/null
    rm -f /tmp/headunit_tidal.sock
fi
if [ -n "$SPOTIFY_PID" ]; then
    kill "$SPOTIFY_PID" 2>/dev/null
    rm -f /tmp/headunit_spotify.sock
fi

exit $EXIT_CODE
