#!/bin/bash
# Script to restore lnxCAD rescue configuration
# Usage: ./restore_lnxcad_config.sh

set -e

echo "=== Restoring lnxCAD rescue configuration ==="

# Copy saved configuration
if [ -f "configs/lnxcad_rescue_defconfig" ]; then
    cp configs/lnxcad_rescue_defconfig .config
    echo "✓ Configuration restored from configs/lnxcad_rescue_defconfig"
else
    echo "✗ Error: configs/lnxcad_rescue_defconfig not found"
    exit 1
fi

# Verify source code modifications are present
echo ""
echo "=== Verifying source code modifications ==="

# Check libbb/appletlib.c
if grep -q "putenv(\"PS1=root:\$PWD# \");" libbb/appletlib.c; then
    echo "✓ libbb/appletlib.c: Custom prompt OK"
else
    echo "⚠ libbb/appletlib.c: Custom prompt missing"
fi

# Check shell/ash.c
if grep -q "custom lnxCAD version" shell/ash.c; then
    echo "✓ shell/ash.c: Custom banner OK"
else
    echo "⚠ shell/ash.c: Custom banner missing"
fi

# Check libbb/messages.c
if grep -q 'const char bb_banner\[\] ALIGN1 = "BusyBox v" BB_VER;' libbb/messages.c; then
    echo "✓ libbb/messages.c: Version without timestamp OK"
else
    echo "⚠ libbb/messages.c: Version without timestamp missing"
fi

echo ""
echo "=== Configuration ready ==="
echo "You can now compile with:"
echo "  make -j\$(nproc) CC=musl-gcc CFLAGS=\"-Os -static\" LDFLAGS=\"-static\""
echo "  strip --strip-all busybox"
echo "  sstrip busybox  # optional"

# Made with Bob
