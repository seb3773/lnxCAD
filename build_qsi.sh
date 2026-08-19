#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# lnxCAD - Q4OS Installer (.qsi) Builder
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
QSI_DIR="$SCRIPT_DIR/qsi_setup"
DEB_DIR="$QSI_DIR/deb_packages"
OUT_DIR="$QSI_DIR/output"
TEMPLATES_DIR="$QSI_DIR/setup_templates"

echo "=================================================="
echo " lnxCAD - Q4OS .qsi Package Builder"
echo "=================================================="

# 1. Check required tools
if ! command -v build-qinstaller >/dev/null 2>&1; then
    echo "[Error] 'build-qinstaller' not found." >&2
    echo "Please install it: sudo apt install q4os-devpack-base" >&2
    exit 1
fi

if ! command -v dpkg-deb >/dev/null 2>&1; then
    echo "[Error] 'dpkg-deb' not found." >&2
    exit 1
fi

# 2. Locate or build Debian package
find_latest_deb() {
    local debs=("$SCRIPT_DIR"/lnxcad_[0-9]*_*.deb "$SCRIPT_DIR"/*.deb)
    for f in "${debs[@]}"; do
        if [ -f "$f" ]; then
            echo "$f"
            return 0
        fi
    done
    return 1
}

LATEST_DEB=""
if find_latest_deb >/dev/null 2>&1; then
    LATEST_DEB="$(find_latest_deb)"
else
    echo "[Info] No .deb package found. Building Debian package first..."
    "$SCRIPT_DIR/build_deb.sh"
    LATEST_DEB="$(find_latest_deb)"
fi

if [ -z "$LATEST_DEB" ] || [ ! -f "$LATEST_DEB" ]; then
    echo "[Error] Failed to find or generate a valid .deb package!" >&2
    exit 1
fi

# 3. Extract package metadata
PACKAGE_NAME=$(dpkg-deb -f "$LATEST_DEB" Package 2>/dev/null || echo "lnxcad")
APP_VERSION=$(dpkg-deb -f "$LATEST_DEB" Version 2>/dev/null || echo "1.0")
DEB_FILENAME=$(basename "$LATEST_DEB")

echo "[Info] Debian package detected: $DEB_FILENAME"
echo "       Package: $PACKAGE_NAME"
echo "       Version: $APP_VERSION"

# 4. Prepare staging directories
mkdir -p "$DEB_DIR" "$OUT_DIR" "$TEMPLATES_DIR"
rm -f "$DEB_DIR"/*.deb "$OUT_DIR"/*.qsi

cp -a "$LATEST_DEB" "$DEB_DIR/"

# Ensure hook permissions
if [ -f "$TEMPLATES_DIR/qch_postsetupr.dvt" ]; then
    chmod +x "$TEMPLATES_DIR/qch_postsetupr.dvt"
fi

# 5. Generate qinstaller configuration with full absolute paths
cat <<EOF > "$QSI_DIR/qinstaller"
#***q4os*setup*config*header*do*not*delete*it***#
PK_NAME="$PACKAGE_NAME"
APPNAME_DESC="lnxCAD Emergency Rescue GUI"
APP_ICON="utilities-terminal"
PK_VERS="$APP_VERSION"
SETUP_TYPE="2"
INST_DEBS="$PACKAGE_NAME"
DEBPCKS_DIR="$DEB_DIR"
TEMPLATES_DIR="$TEMPLATES_DIR"
OUT_DIR="$OUT_DIR"
APPLNK_ENTRY="0"
DESKTOP_ENTRY="0"
MENU_ENTRY="0"
DSTR_BASE="debian;ubuntu"
DSTR_EDTN="bullseye;bookworm;trixie;jammy;noble"
Q4VER_MIN="4.0"
CHK_INET="0"
EOF

# 6. Execute Q4OS installer generator
echo "[Info] Running build-qinstaller..."
(
    cd "$QSI_DIR"
    build-qinstaller qinstaller
)

# 7. Finalize and copy .qsi package to project root
find_latest_qsi() {
    local qsis=("$OUT_DIR"/*.qsi)
    for f in "${qsis[@]}"; do
        if [ -f "$f" ]; then
            echo "$f"
            return 0
        fi
    done
    return 1
}

if find_latest_qsi >/dev/null 2>&1; then
    LATEST_QSI="$(find_latest_qsi)"
    FINAL_QSI_NAME=$(basename "$LATEST_QSI")
    cp -a "$LATEST_QSI" "$SCRIPT_DIR/$FINAL_QSI_NAME"
    chmod +x "$SCRIPT_DIR/$FINAL_QSI_NAME"
    echo ""
    echo "=================================================="
    echo " SUCCESS: Q4OS Installer successfully generated!"
    echo " File : $SCRIPT_DIR/$FINAL_QSI_NAME"
    echo " Size : $(ls -lh "$SCRIPT_DIR/$FINAL_QSI_NAME" | awk '{print $5}')"
    echo "=================================================="
else
    echo "[Error] Failed to generate .qsi file." >&2
    exit 1
fi
