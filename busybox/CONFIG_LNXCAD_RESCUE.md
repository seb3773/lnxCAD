# BusyBox lnxCAD Rescue Configuration

## Overview

Ultra-minimal BusyBox configuration for emergency rescue shell.

**Features:**
- Static binary (musl-gcc)
- 29 essential applets
- Size: ~329 KB (with sstrip)
- Line editing + history (15 commands)
- Dynamic prompt: `root:/current/path# `
- Custom banner

## Included Applets (29)

### File Management (9)
- `ls` - List files/directories
- `cp` - Copy files
- `mv` - Move/rename files
- `rm` - Remove files
- `cat` - Display content
- `mkdir` - Create directories
- `find` - Search files
- `chmod` - Modify permissions
- `chown` - Change owner

### Archiving (1)
- `tar` - Archive/extract (gzip, bzip2)

### Disk Tools (5)
- `dd` - Low-level copy
- `hexdump` - Hexadecimal dump
- `blkid` - Identify partitions
- `losetup` - Manage loop devices
- `mount` - Mount filesystems

### Text Processing (4)
- `grep` - Search patterns
- `sed` - Stream editing
- `awk` - Advanced text processing
- `xargs` - Build commands

### Network (2)
- `ip` - Network configuration
- `ping` - Test connectivity

### Shell and System (8)
- `sh` (ash) - Shell with line editing
- `echo` - Display text
- `env` - Environment variables
- `pwd` - Current directory
- `cd` - Change directory (builtin)
- `export` - Export variables (builtin)
- `test` / `[` - Conditional tests
- `true` - Return success

## Configuration Files

### 1. Complete Configuration
**File:** `configs/lnxcad_rescue_defconfig`
- Complete `.config` configuration (1800+ lines)
- All BusyBox options
- Ready to copy to `.config`

### 2. Restore Script
**File:** `restore_lnxcad_config.sh`
```bash
./restore_lnxcad_config.sh
```
- Restores configuration
- Verifies source code modifications
- Displays compilation instructions

## Source Code Modifications

### 1. Direct Shell Launch (libbb/appletlib.c)
**Line:** ~1120-1130

```c
if (!argv[1]) {
    extern int ash_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
    putenv("PS1=root:$PWD# ");
    char *sh_argv[] = { "sh", NULL };
    return ash_main(1, sh_argv);
}
```

**Effect:** Launches shell directly with custom prompt

### 2. Custom Banner (shell/ash.c)
**Line:** ~10155

```c
out1fmt("%s %s - custom lnxCAD version\n",
    bb_banner,
    "built-in shell (ash)"
);
```

**Effect:** Displays "BusyBox v1.39.0.git built-in shell (ash) - custom lnxCAD version"

### 3. Version Without Timestamp (libbb/messages.c)
**Line:** ~14

```c
const char bb_banner[] ALIGN1 = "BusyBox v" BB_VER;
```

**Effect:** Clean version without compilation date/time

## Key Configuration Options

### Static Compilation
```
CONFIG_STATIC=y
CONFIG_PIE=n
```

### Shell (ash)
```
CONFIG_ASH=y
CONFIG_ASH_OPTIMIZE_FOR_SIZE=y
CONFIG_ASH_EXPAND_PRMT=y          # Prompt expansion ($PWD)
CONFIG_FEATURE_EDITING=y          # Line editing
CONFIG_FEATURE_EDITING_MAX_LEN=1024
CONFIG_FEATURE_EDITING_HISTORY=15 # 15 command history
```

### Size Optimizations
```
CONFIG_FEATURE_VERBOSE_USAGE=n    # No detailed help
CONFIG_FEATURE_COMPRESS_USAGE=n   # No help compression
CONFIG_LONG_OPTS=n                # No long options
CONFIG_FEATURE_DEVFS=n            # No devfs
```

### Minimal Network
```
CONFIG_FEATURE_IPV6=n             # No IPv6
CONFIG_FEATURE_IFUPDOWN_IPV6=n
```

## Compilation

### Prerequisites
```bash
sudo apt-get install musl-tools musl-dev
```

### Required Symlinks (if header errors occur)
```bash
cd /usr/include/x86_64-linux-musl/
sudo ln -sf /usr/include/linux linux
sudo ln -sf /usr/include/x86_64-linux-gnu/asm asm
sudo ln -sf /usr/include/asm-generic asm-generic
```

### Compilation Commands
```bash
# 1. Restore configuration
./restore_lnxcad_config.sh

# 2. Compile
make -j$(nproc) CC=musl-gcc CFLAGS="-Os -static" LDFLAGS="-static"

# 3. Optimize size
strip --strip-all busybox
sstrip busybox  # optional, if available

# 4. Verify
ls -lh busybox
ldd busybox  # should display "not a dynamic executable"
```

### Automated Script
```bash
./build_busybox.sh
```

## Usage

### Launch
```bash
./busybox
```
Displays banner and launches shell with prompt `root:/current/path# `

### Individual Applets
```bash
./busybox ls -la
./busybox tar czf backup.tar.gz /data
./busybox dd if=/dev/sda of=disk.img bs=4M
```

### Create Symlinks
```bash
mkdir -p bin
cd bin
for applet in $(../busybox --list); do
    ln -s ../busybox $applet
done
```

## Shell Features

### Line Editing
- **Left/Right arrows:** Move cursor
- **Up/Down arrows:** Navigate history (15 commands)
- **Ctrl+A:** Beginning of line
- **Ctrl+E:** End of line
- **Ctrl+U:** Clear before cursor
- **Ctrl+K:** Clear after cursor
- **Tab:** Completion (if enabled)

### Dynamic Prompt
Prompt displays current path:
```
root:/# cd /tmp
root:/tmp# cd /var/log
root:/var/log# 
```

## Binary Size

| Version | Size | Notes |
|---------|------|-------|
| Unstripped | ~1.2 MB | With debug symbols |
| strip --strip-all | ~332 KB | Symbols removed |
| + sstrip | ~329 KB | Maximum optimization |

## Restore After git clean

If you do a `git clean` or lose configuration:

```bash
# 1. Restore configuration
./restore_lnxcad_config.sh

# 2. Verify source code modifications
# Script displays verifications

# 3. If modifications are missing, reapply them:
# - libbb/appletlib.c (prompt)
# - shell/ash.c (banner)
# - libbb/messages.c (version)

# 4. Recompile
make -j$(nproc) CC=musl-gcc CFLAGS="-Os -static" LDFLAGS="-static"
strip --strip-all busybox
sstrip busybox
```

## Important Notes

1. **Source Code Modifications:** The 3 modified files are NOT in `.config`. They must be preserved or reapplied after `git clean`.

2. **Configuration vs Code:** 
   - `.config` = compilation options
   - C modifications = custom behavior

3. **Complete Backup:** For total backup, keep:
   - `configs/lnxcad_rescue_defconfig`
   - The 3 modified C files (or their diffs)
   - `restore_lnxcad_config.sh`

4. **Test After Compilation:**
   ```bash
   ./busybox
   # Verify: banner, prompt, line editing, history
   ```

## Troubleshooting

### Error: "linux/types.h: No such file"
```bash
cd /usr/include/x86_64-linux-musl/
sudo ln -sf /usr/include/linux linux
```

### Prompt Displays "$PWD" Literally
Check that `CONFIG_ASH_EXPAND_PRMT=y` in `.config`

### No Line Editing
Verify:
```
CONFIG_FEATURE_EDITING=y
CONFIG_FEATURE_EDITING_HISTORY=15
```

### Binary Too Large
- Verify `CONFIG_STATIC=y`
- Use `strip --strip-all`
- Install and use `sstrip`

## Author

Configuration created for lnxCAD rescue shell
Date: June 2026