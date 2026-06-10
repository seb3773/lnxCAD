# Quick Start - BusyBox lnxCAD Rescue

## Quick Configuration Restore

### 1. Restore Configuration
```bash
./restore_lnxcad_config.sh
```

### 2. Compile
```bash
make -j$(nproc) CC=musl-gcc CFLAGS="-Os -static" LDFLAGS="-static"
strip --strip-all busybox
sstrip busybox  # optional
```

### 3. Verify
```bash
ls -lh busybox          # Should display ~329K
ldd busybox             # Should display "not a dynamic executable"
./busybox               # Launch shell with custom prompt
```

## Backup Files

- **`configs/lnxcad_rescue_defconfig`** - Complete configuration (.config)
- **`patches_lnxcad.txt`** - Source code patches (3 files)
- **`restore_lnxcad_config.sh`** - Automatic restore script
- **`CONFIG_LNXCAD_RESCUE.md`** - Complete documentation

## If Patches Are Lost

Verify and reapply if necessary:

```bash
# Verify
grep -n "putenv(\"PS1=root:\$PWD# \");" libbb/appletlib.c
grep -n "custom lnxCAD version" shell/ash.c
grep -n 'const char bb_banner\[\] ALIGN1 = "BusyBox v" BB_VER;' libbb/messages.c

# If missing, see patches_lnxcad.txt to apply them
```

## Build Characteristics

- **Size:** ~329 KB (static, musl, sstrip)
- **Applets:** 29 essential tools
- **Shell:** ash with line editing + history (15 commands)
- **Prompt:** `root:/current/path# `
- **Banner:** `BusyBox v1.39.0.git built-in shell (ash) - custom lnxCAD version`

## List of 29 Applets

```
ls cp mv rm cat mkdir find chmod chown    # Files (9)
tar                                        # Archive (1)
dd hexdump blkid losetup mount            # Disk (5)
grep sed awk xargs                        # Text (4)
ip ping                                    # Network (2)
sh echo env pwd cd export test true       # Shell (8)
```

## Usage

```bash
# Launch shell
./busybox

# Use specific applet
./busybox ls -la
./busybox tar czf backup.tar.gz /data

# Create symlinks (optional)
mkdir -p bin && cd bin
for app in $(../busybox --list); do ln -s ../busybox $app; done
```

## Quick Troubleshooting

**Error: linux/types.h not found**
```bash
cd /usr/include/x86_64-linux-musl/
sudo ln -sf /usr/include/linux linux
sudo ln -sf /usr/include/x86_64-linux-gnu/asm asm
sudo ln -sf /usr/include/asm-generic asm-generic
```

**Prompt displays "$PWD" literally**
```bash
# Check in .config:
grep CONFIG_ASH_EXPAND_PRMT .config
# Should display: CONFIG_ASH_EXPAND_PRMT=y
```

**No line editing**
```bash
# Check in .config:
grep CONFIG_FEATURE_EDITING .config
# Should display: CONFIG_FEATURE_EDITING=y