#!/bin/bash
# Minimal EGLFS display test for Phase 2 verification
# Usage: ./run-eglfs-test.sh
#
# Method 1: tries compiled binary (if built)
# Method 2: falls back to qml runtime

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export QT_QPA_PLATFORM=eglfs
export QT_AUTO_SCREEN_SCALE_FACTOR=0
export QT_SCALE_FACTOR=1
export QSG_RHI_BACKEND=opengl

# Try compiled binary first
if [ -f "$SCRIPT_DIR/build/eglfs-test" ]; then
    echo "Running compiled eglfs-test..."
    exec "$SCRIPT_DIR/build/eglfs-test"
fi

# Fallback: use qml runtime
if command -v qml &>/dev/null; then
    echo "Running via qml runtime..."
    exec qml "$SCRIPT_DIR/main.qml"
fi

# Neither available — build it
if command -v cmake &>/dev/null && command -v make &>/dev/null; then
    echo "Building eglfs-test..."
    mkdir -p "$SCRIPT_DIR/build"
    cd "$SCRIPT_DIR/build"
    cmake .. && make -j$(nproc)
    if [ $? -eq 0 ]; then
        echo "Build succeeded, running..."
        exec ./eglfs-test
    else
        echo "Build failed"
        exit 1
    fi
fi

echo "ERROR: No qml runtime, no compiled binary, and no build tools available."
echo "Install qt6-declarative-tools or build with cmake."
exit 1
