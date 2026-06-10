# BusyBox Custom Build - lnxCAD Edition

## Overview

This is a minimal, statically-linked BusyBox build optimized for emergency rescue operations. The binary is compiled with musl libc for maximum portability and minimal size (~326KB).

## Key Features

- **Static binary**: No external dependencies, works in degraded environments
- **Minimal size**: ~326KB (stripped with sstrip)
- **29 essential applets**: Core utilities for system rescue
- **Custom shell prompt**: `root:<current_path># `
- **Direct shell launch**: Running `./busybox` without arguments opens the shell directly
- **Custom banner**: Clean, professional startup message

## Build Requirements

- `musl-gcc` (musl-tools package)
- `make`
- `strip` (binutils)
- `sstrip` (optional, for extra size reduction)

Install on Debian/Ubuntu:
```bash
sudo apt install musl-tools build-essential
sudo apt install sstrip  # optional
```

## Building

### Quick Build
```bash
chmod +x build_busybox.sh
./build_busybox.sh
```

### Manual Build
```bash
make clean
make -j$(nproc) CC=musl-gcc CFLAGS="-Os -static" LDFLAGS="-static"
strip --strip-all busybox
sstrip busybox  # optional
```

## Included Applets (29)

Essential utilities for rescue operations:

**Shell & Core:**
- `ash`, `sh` - Shell
- `echo`, `cat`, `ls`, `cp`, `mv`, `rm`, `mkdir` - File operations

**System Info:**
- `ps`, `kill`, `dmesg`, `uname`, `stat`, `df` - Process & system info

**File Search & Processing:**
- `find`, `grep`, `sed`, `awk`, `xargs`, `tail` - Text processing

**Disk & Network:**
- `mount`, `umount`, `losetup`, `blkid`, `hexdump` - Disk utilities
- `ip`, `ifconfig`, `ping` - Network utilities

## Code Modifications

### 1. Direct Shell Launch (`libbb/appletlib.c`)
When launched without arguments, busybox directly calls `ash_main()` instead of showing help:
```c
if (!argv[1]) {
    extern int ash_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
    putenv("PS1=root:$PWD# ");
    char *sh_argv[] = { "sh", NULL };
    return ash_main(1, sh_argv);
}
```

### 2. Custom Banner (`shell/ash.c`)
Removed empty lines and added custom branding:
```c
out1fmt("%s %s - custom lnxCAD version\n",
    bb_banner,
    "built-in shell (ash)"
);
```

### 3. Version String (`libbb/messages.c`)
Removed compilation timestamp:
```c
const char bb_banner[] ALIGN1 = "BusyBox v" BB_VER;
```

### 4. Prompt Expansion (`.config`)
Enabled prompt expansion for dynamic path display:
```
CONFIG_ASH_EXPAND_PRMT=y
```

### 5. Musl Headers Symlinks
Created symlinks for kernel headers in musl include directory:
```bash
sudo ln -sf /usr/include/linux /usr/include/x86_64-linux-musl/linux
sudo ln -sf /usr/include/asm-generic /usr/include/x86_64-linux-musl/asm-generic
sudo ln -sf /usr/include/x86_64-linux-gnu/asm /usr/include/x86_64-linux-musl/asm
```

## Configuration

The build uses a minimal configuration (`configure_ultra_minimal_v3.sh`) with:
- Static linking enabled (`CONFIG_STATIC=y`)
- Help/usage disabled (size optimization)
- Only essential applets enabled
- Prompt expansion enabled

## Usage

### Launch Shell
```bash
./busybox
```
Output:
```
BusyBox v1.39.0.git built-in shell (ash) - custom lnxCAD version
root:/current/path# 
```

### Run Specific Applet
```bash
./busybox ls -la
./busybox mount /dev/sda1 /mnt
./busybox ip addr show
```

### List All Applets
```bash
./busybox --list
```

## Verification

### Check Static Linking
```bash
ldd busybox
# Output: "not a dynamic executable"
```

### Check Binary Size
```bash
ls -lh busybox
file busybox
```

### Test Functionality
```bash
echo "pwd; ls; exit" | ./busybox
```

## Use Cases

This build is designed for:
- Emergency system recovery
- Minimal rescue environments
- Initramfs/initrd
- Corrupted system repair
- Environments without `/lib` or `/usr/lib`
- Chroot environments
- Embedded systems

## Size Comparison

| Build Type | Size | Notes |
|------------|------|-------|
| glibc static | 1.3M | Standard static build |
| glibc static + strip | 1.3M | After strip |
| musl static | 326K | With musl-gcc |
| musl + strip | 326K | After strip --strip-all |
| musl + sstrip | ~325K | After sstrip (optional) |

## Files Modified

- `libbb/appletlib.c` - Direct shell launch logic
- `shell/ash.c` - Custom banner
- `libbb/messages.c` - Version string without timestamp
- `.config` - Build configuration

## Scripts

- `build_busybox.sh` - Complete build script
- `configure_ultra_minimal_v3.sh` - Configuration generator
- `compile_with_musl_v2.sh` - Musl compilation script

## License

BusyBox is licensed under GPLv2. See the LICENSE file in the source tree.

## Author

Custom modifications for lnxCAD project.