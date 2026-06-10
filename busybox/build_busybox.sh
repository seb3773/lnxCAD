#!/bin/bash
# BusyBox Custom Build Script for lnxCAD
# Builds a minimal, static BusyBox binary with musl libc

set -e

echo "=========================================="
echo "BusyBox Custom Build - lnxCAD Edition"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check for required tools
echo "Checking build requirements..."
if ! command -v musl-gcc &> /dev/null; then
    echo -e "${RED}ERROR: musl-gcc not found. Install with: sudo apt install musl-tools${NC}"
    exit 1
fi
echo -e "${GREEN}✓ musl-gcc found${NC}"

# Check for optional sstrip
SSTRIP_AVAILABLE=false
if command -v sstrip &> /dev/null; then
    SSTRIP_AVAILABLE=true
    echo -e "${GREEN}✓ sstrip found (will be used for extra size reduction)${NC}"
else
    echo -e "${YELLOW}⚠ sstrip not found (optional, install with: sudo apt install sstrip)${NC}"
fi
echo ""

# Clean previous build
echo "Cleaning previous build..."
make clean > /dev/null 2>&1 || true
make distclean > /dev/null 2>&1 || true
echo -e "${GREEN}✓ Clean complete${NC}"
echo ""

# Load configuration
echo "Loading custom configuration..."
if [ ! -f .config ]; then
    echo -e "${RED}ERROR: .config not found. Run configure_ultra_minimal_v3.sh first${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Configuration loaded${NC}"
echo ""

# Build with musl
echo "Building BusyBox with musl-gcc..."
echo "  Compiler: musl-gcc"
echo "  CFLAGS: -Os -static"
echo "  LDFLAGS: -static"
echo ""

make -j$(nproc) \
    CC=musl-gcc \
    HOSTCC=gcc \
    CFLAGS="-Os -static" \
    LDFLAGS="-static" 2>&1 | tee build.log | tail -10

if [ ! -f busybox ]; then
    echo -e "${RED}ERROR: Build failed. Check build.log for details${NC}"
    exit 1
fi
echo ""
echo -e "${GREEN}✓ Build successful${NC}"
echo ""

# Strip binary
echo "Stripping binary..."
SIZE_BEFORE=$(stat -c%s busybox)
strip --strip-all busybox
SIZE_AFTER_STRIP=$(stat -c%s busybox)
SAVED_STRIP=$((SIZE_BEFORE - SIZE_AFTER_STRIP))
echo -e "${GREEN}✓ strip: saved ${SAVED_STRIP} bytes${NC}"

# Optional sstrip
if [ "$SSTRIP_AVAILABLE" = true ]; then
    SIZE_BEFORE_SSTRIP=$(stat -c%s busybox)
    sstrip busybox
    SIZE_AFTER_SSTRIP=$(stat -c%s busybox)
    SAVED_SSTRIP=$((SIZE_BEFORE_SSTRIP - SIZE_AFTER_SSTRIP))
    echo -e "${GREEN}✓ sstrip: saved ${SAVED_SSTRIP} bytes${NC}"
fi
echo ""

# Final verification
echo "=========================================="
echo "Build Summary"
echo "=========================================="
ls -lh busybox | awk '{print "Binary size: " $5}'
file busybox | sed 's/^/Type: /'

# Verify static linking
if ldd busybox 2>&1 | grep -q "not a dynamic executable"; then
    echo -e "${GREEN}✓ Static linking: confirmed${NC}"
else
    echo -e "${RED}✗ Warning: binary may not be fully static${NC}"
    ldd busybox 2>&1 | head -5
fi

# Count applets
APPLET_COUNT=$(./busybox --list 2>/dev/null | wc -l)
echo "Applets: ${APPLET_COUNT}"
echo ""

# Test basic functionality
echo "Testing basic functionality..."
if ./busybox echo "test" > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Basic test passed${NC}"
else
    echo -e "${RED}✗ Basic test failed${NC}"
    exit 1
fi
echo ""

echo "=========================================="
echo -e "${GREEN}Build completed successfully!${NC}"
echo "=========================================="
echo ""
echo "Binary location: ./busybox"
echo "Build log: ./build.log"
echo ""
echo "To test the shell:"
echo "  ./busybox"
echo ""
echo "To list all applets:"
echo "  ./busybox --list"

# Made with Bob
