#!/bin/bash
# ============================================================================
# QNX GLOBAL E-HACK 2026 - ONE-CLICK BUILD SCRIPT
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "============================================================"
echo "🚀 Building QNX Autonomous SDV Project"
echo "============================================================"

# Check if QNX SDP environment is sourced
if [ -z "$QNX_HOST" ]; then
    if [ -f "/home/rohith/qnx800/qnxsdp-env.sh" ]; then
        echo "📦 Sourcing QNX 8.0 SDP environment from /home/rohith/qnx800/qnxsdp-env.sh..."
        source /home/rohith/qnx800/qnxsdp-env.sh
    else
        echo "⚠️  QNX SDP not found in default path. Proceeding with host build only."
    fi
fi

# Clean previous build
make clean || true

# Build Host Binary
echo ""
echo "[1/2] Building Host Simulation..."
make host

# Build QNX Pi 4 Target Binary if qcc is present
if command -v qcc &> /dev/null; then
    echo ""
    echo "[2/2] Cross-compiling for QNX Pi 4 (ARM64)..."
    make pi4
    echo ""
    echo "============================================================"
    echo "🎉 ALL BUILDS COMPLETED SUCCESSFULLY!"
    echo "   - Host Simulator : build/sdv_host"
    echo "   - QNX Pi 4 Target: build/sdv_pi4"
    echo "============================================================"
else
    echo "⚠️  qcc cross-compiler not found in PATH. Pi 4 target skipped."
fi
