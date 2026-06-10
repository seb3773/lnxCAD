# lnxCAD Compile-Time Theming System

This document describes the design, configuration options, and resources of the compile-time theming system implemented for the lnxCAD rescue GUI (`cad_rescue_gui`). 

To avoid runtime overhead, memory allocations, or dependency on heavy rendering libraries (like FreeType or libpng at runtime), all visual assets (icons, fonts, cursors) and styles (colors, window sizes) are processed at compile time into compact freestanding C structures.

---

## 1. Directory Structure

Themes are stored in `assets/<theme_name>/` directories. A complete theme directory must contain the following files:

```
assets/<theme_name>/
├── cad_theme.conf    # Color values and window layout instructions
├── font.ttf          # TTF Font file used for the UI
├── pointer           # Xcursor pointer file for the mouse cursor
└── *.png             # 15 UI Icons (listed below)
```

### Required Icons (.png format)

- `sleep.png` — Sleep option icon
- `power.png` — Power options icon
- `restart.png` — Restart icon
- `cadshutd.png` — Shutdown/Main button icon
- `terminal.png` — Terminal icon
- `find.png` — Search icon
- `check1.png` — Checkbox checked state icon
- `check2.png` — Checkbox unchecked state icon
- `close.png` — Close button icon
- `runcommand.png` — Run command icon
- `console.png` — Console/Session icon
- `taskmgr.png` — Task manager icon
- `password.png` — Privilege elevation password icon
- `branding.png` *(Optional)* — Corporate logo drawn in the top-right corner. If absent, branding is disabled at compile time.

---

## 2. Resource Specifications & Constraints

### A. Icons (PNG)
- All icons must be in 8-bit RGBA PNG format.
- Recommended size for main icons is 24x24 pixels.
- The `branding.png` (if provided) can be any arbitrary dimensions.

### B. Font (TTF)
- A standard TrueType Font (`font.ttf`).
- Cooked into `custom_font.h` at compile time and loaded by Nuklear's rawfb font renderer.

### C. Cursor (Xcursor `pointer`)
- A standard Xcursor binary file (containing at least one cursor of size 48x48 pixels).
- The compiler extracts the 48x48 pre-multiplied ARGB image along with hotspot coordinates (`xhot`, `yhot`) and cooks them into `cursor_data.h`.

---

## 3. Configuration File (`cad_theme.conf`)

The configuration file is a simple key-value `INI`-style file. It defines visual colors and centered layouts.

### A. Colors (RGBA format)
Colors are written as comma-separated `R, G, B, A` integers (0-255).

- `color_bg` — Primary background color for all window dialogs.
- `color_title_bg` — Background color for window title bars.
- `color_hover` — Highlight color for buttons/elements under cursor hover.
- `color_active` — Highlight color for buttons/elements in pressed/active state.
- `color_curtain` — Fullscreen modal overlay (blocks inputs when a password dialog is active).

### B. Window Layout
All diagnostic windows are automatically centered on the screen. Their dimensions are specified in the format `numerator, denominator` (or absolute pixels if `denominator` is 0).

Example configuration:
```ini
size_terminal = 5, 9, 5, 9  # Width = 5/9 of screen width, Height = 5/9 of screen height.
size_run = 550, 0, 240, 0   # Width = 550px, Height = 240px.
```

Supported window layout size keys:
- `size_terminal` — Diagnostic terminal size
- `size_taskmgr` — Task manager size
- `size_run` — Run command prompt size
- `size_pwd` — Root privilege elevation prompt size
- `size_cp` — Change password prompt size
- `size_su` — Switch user session dialog size

*Note: The main system menu and the bottom-right power/start menu have their layouts and dimensions calculated dynamically at runtime to match font bounds and button sizes, and only respect the background and hover colors of the theme.*

---

## 4. How to Build/Switch Themes

A compiler wrapper script `build.sh` is provided in the project root to automate the generation of code headers and compilation of the executable.

### Syntax:
```bash
./build.sh [theme_name] [compress_mode]
```

- **`theme_name`**: Name of the theme folder under `assets/` (`base` | `color` | `tech`). Defaults to `base`.
- **`compress_mode`**: Decompression library choice for the final packaging (`lz4` for rapid development, `zx0` for maximum production size reduction). Defaults to `lz4`.

### Example:
```bash
# Compile the dark theme using lz4 compression
./build.sh tech lz4
```

The script does the following:
1. Copies the theme assets to the temporary working directory `assets_build/`.
2. Compiles and runs `theme_generator` to generate `cad_theme.h`.
3. Runs `python3 convert_pngs.py` to compile all PNGs, font, and pointer cursor into C headers.
4. Performs a `make clean` and invokes `make` with the chosen compression algorithm.
