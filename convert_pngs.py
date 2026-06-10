import sys
from PIL import Image

def convert(filename, out_filename, name, w_macro, h_macro):
    img = Image.open(filename).convert('RGBA')
    width, height = img.size
    
    with open(out_filename, 'w') as f:
        f.write(f"#define {w_macro} {width}\n")
        f.write(f"#define {h_macro} {height}\n")
        f.write(f"static const uint32_t {name}[{width*height}] = {{\n")
        
        pixels = list(img.getdata())
        count = 0
        for y in range(height):
            f.write("    ")
            for x in range(width):
                r, g, b, a = pixels[y * width + x]
                val = (a << 24) | (r << 16) | (g << 8) | b
                f.write(f"0x{val:08X}, ")
                count += 1
            f.write("\n")
        f.write("};\n")

convert('assets_build/sleep.png', 'sleep_icon.h', 'sleep_data', 'SLEEP_W', 'SLEEP_H')
convert('assets_build/power.png', 'power_icon.h', 'power_data', 'POWER_W', 'POWER_H')
convert('assets_build/restart.png', 'restart_icon.h', 'restart_data', 'RESTART_W', 'RESTART_H')
convert('assets_build/cadshutd.png', 'shutdown_icon.h', 'shutdown_data', 'SHUTDOWN_W', 'SHUTDOWN_H')
convert('assets_build/terminal.png', 'terminal_icon.h', 'terminal_data', 'TERMINAL_W', 'TERMINAL_H')
convert('assets_build/find.png', 'find_icon.h', 'find_data', 'FIND_W', 'FIND_H')
convert('assets_build/check1.png', 'check1_icon.h', 'check1_data', 'CHECK1_W', 'CHECK1_H')
convert('assets_build/check2.png', 'check2_icon.h', 'check2_data', 'CHECK2_W', 'CHECK2_H')
convert('assets_build/close.png', 'close_icon.h', 'close_data', 'CLOSE_W', 'CLOSE_H')
convert('assets_build/runcommand.png', 'runcommand_icon.h', 'runcommand_data', 'RUNCOMMAND_W', 'RUNCOMMAND_H')
convert('assets_build/console.png', 'console_icon.h', 'console_data', 'CONSOLE_W', 'CONSOLE_H')
convert('assets_build/taskmgr.png', 'taskmgr_icon.h', 'taskmgr_data', 'TASKMGR_W', 'TASKMGR_H')
convert('assets_build/password.png', 'password_icon.h', 'password_data', 'PASSWORD_W', 'PASSWORD_H')

import os
branding_png = 'assets_build/branding.png'
branding_h = 'branding_icon.h'
if os.path.exists(branding_png):
    print("Converting branding.png...")
    convert(branding_png, branding_h, 'branding_data', 'BRANDING_W', 'BRANDING_H')
    with open(branding_h, 'r') as f:
        content = f.read()
    with open(branding_h, 'w') as f:
        f.write("#define HAS_BRANDING 1\n" + content)
else:
    print("No branding.png found. Writing dummy header...")
    with open(branding_h, 'w') as f:
        f.write("#define HAS_BRANDING 0\n")

# Conversion de la police TTF
import struct
def convert_font(font_path, out_filename):
    print(f"Converting font {font_path}...")
    with open(font_path, 'rb') as f:
        data = f.read()
    with open(out_filename, 'w') as f:
        f.write("unsigned char font_ttf[] = {\n")
        for i in range(0, len(data), 12):
            chunk = data[i : i+12]
            f.write("    " + "".join(f"0x{b:02x}, " for b in chunk) + "\n")
        f.write("};\n")
        f.write(f"unsigned int font_ttf_len = {len(data)};\n")

# Extraction du curseur Xcursor pointer
def parse_xcursor(filename, out_filename):
    print(f"Extracting cursor from {filename}...")
    with open(filename, 'rb') as f:
        data = f.read()
    
    if len(data) < 16:
        raise ValueError("Xcursor file too small")
        
    magic, header_size, version, toc_count = struct.unpack('<4sIII', data[:16])
    if magic != b'Xcur':
        raise ValueError("Not an Xcursor file")
        
    toc_offset = 16
    best_offset = None
    best_size = 0
    target_size = 48
    
    for i in range(toc_count):
        entry_data = data[toc_offset + i*12 : toc_offset + (i+1)*12]
        if len(entry_data) < 12:
            break
        t_type, t_subtype, t_position = struct.unpack('<III', entry_data)
        if t_type == 0xfffd0002:
            if t_subtype == target_size:
                best_offset = t_position
                best_size = t_subtype
                break
            elif best_offset is None or abs(int(t_subtype) - target_size) < abs(int(best_size) - target_size):
                best_offset = t_position
                best_size = t_subtype
                
    if best_offset is None:
        raise ValueError("No image chunk found in Xcursor file")
        
    chunk_header = data[best_offset : best_offset + 36]
    c_header_size, c_type, c_subtype, c_version, width, height, xhot, yhot, delay = struct.unpack('<IIIIIIIII', chunk_header)
    
    pixel_offset = best_offset + c_header_size
    pixel_count = width * height
    pixel_data = data[pixel_offset : pixel_offset + pixel_count * 4]
    
    with open(out_filename, 'w') as f:
        f.write("/* Cursor extracted from Xcursor file - ARGB premultiplied */\n")
        f.write(f"#define CURSOR_W {width}\n")
        f.write(f"#define CURSOR_H {height}\n")
        f.write(f"#define CURSOR_HOT_X {xhot}\n")
        f.write(f"#define CURSOR_HOT_Y {yhot}\n")
        f.write(f"static const uint32_t cursor_data[{pixel_count}] = {{\n")
        
        for y in range(height):
            f.write("    ")
            for x in range(width):
                idx = (y * width + x) * 4
                pixel_bytes = pixel_data[idx : idx + 4]
                val = struct.unpack('<I', pixel_bytes)[0]
                f.write(f"0x{val:08X}, ")
            f.write("\n")
        f.write("};\n")

# Lancer les conversions
if os.path.exists('assets_build/font.ttf'):
    convert_font('assets_build/font.ttf', 'custom_font.h')
if os.path.exists('assets_build/pointer'):
    parse_xcursor('assets_build/pointer', 'cursor_data.h')

print("Done")
