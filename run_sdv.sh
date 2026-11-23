#!/bin/bash
# ============================================================================
# QNX GLOBAL E-HACK 2026 - SDV LAUNCHER SCRIPT
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "============================================================"
echo "🚀 Starting QNX Autonomous SDV Ground Vehicle"
echo "============================================================"

# Determine execution binary (Pi 4 native vs Host simulator)
if [ "$(uname -m)" = "aarch64" ] && [ -f "build/sdv_pi4" ]; then
    EXEC_BIN="build/sdv_pi4"
    echo "🎯 Running Native QNX ARM64 Target Binary..."
elif [ -f "build/sdv_host" ]; then
    EXEC_BIN="build/sdv_host"
    echo "🖥️  Running Host Simulation Binary..."
else
    echo "🔨 Building project binaries first..."
    ./build.sh
    EXEC_BIN="build/sdv_host"
fi

# Check available sensors
echo ""
echo "[Sensor Health Check]"
if [ -e "/dev/video0" ]; then
    echo "  📹 USB Webcam: FOUND (/dev/video0)"
else
    echo "  📹 USB Webcam: Not connected (using synthetic feed)"
fi

if [ -e "/dev/i2c1" ]; then
    echo "  📏 ToF I2C Bus: FOUND (/dev/i2c1)"
elif [ -e "/dev/ser2" ]; then
    echo "  📏 ToF UART LiDAR: FOUND (/dev/ser2)"
else
    echo "  📏 ToF Sensor: Physical bus offline (using high-fidelity simulation)"
fi

if [ -e "/dev/ser1" ]; then
    echo "  📡 V2V Bluetooth: FOUND (/dev/ser1)"
else
    echo "  📡 V2V Bluetooth: Serial device offline (telemetry directed to console)"
fi

echo ""
echo "🚗 Launching $EXEC_BIN..."
echo "Press Ctrl+C to safely halt the vehicle."
echo "============================================================"

exec "$EXEC_BIN" "$@"
