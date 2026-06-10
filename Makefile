STATIC_LIBC ?= musl
DEBUG ?= 0

ifeq ($(DEBUG),1)
    DEBUG_FLAGS = -DENABLE_DEBUG
else
    DEBUG_FLAGS =
endif

ifeq ($(STATIC_LIBC),musl)
    CC = musl-gcc
    DRM_INC = -Iinclude_compat -I/usr/include/libdrm
    LIBS = -lm
    COMPAT_DEP = include_compat/libdrm
else
    CC = gcc
    DRM_INC = -I/usr/include/libdrm
    LIBS = -lm -lutil
    COMPAT_DEP =
endif

# Options de compilation agressives pour minimiser la taille
CFLAGS = -Wall -Wextra -std=gnu99 -g0 -Os -DNDEBUG -fno-ident -fstrict-aliasing -flto -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -fno-unwind-tables -fomit-frame-pointer -fno-stack-protector -fno-math-errno -ffast-math -fvisibility=hidden -fmerge-all-constants -fno-plt -D_GNU_SOURCE $(DEBUG_FLAGS)

# Options d'édition de liens pour minimiser la taille
LDFLAGS = -static -flto -fuse-ld=gold -Wl,--relax -Wl,-z,norelro -Wl,--gc-sections,--build-id=none,--as-needed,--strip-all,-O1,--icf=all,--compress-debug-sections=zlib $(LIBS)

# Sources libtsm
TSM_SRCS = $(wildcard libtsm/src/tsm/*.c) \
           libtsm/src/shared/shl-pty.c \
           libtsm/src/shared/shl-htable.c \
           libtsm/src/shared/shl-ring.c \
           libtsm/external/wcwidth/wcwidth.c

TSM_INC = -Ilibtsm/src/tsm -Ilibtsm/src/shared -Ilibtsm/external -Ilibtsm/external/wcwidth

COMPRESS ?= lz4

ifeq ($(COMPRESS),zx0)
    COMPRESS_FLAGS = -DUSE_ZX0
    DECOMPRESSOR_SRCS = decompressors/zx0/zx0_decompress.c
else
    COMPRESS_FLAGS = -DUSE_LZ4
    DECOMPRESSOR_SRCS = decompressors/lz4/codec_lz4.c
endif

all: lnxcad cad_rescue_gui

include_compat/libdrm:
	mkdir -p include_compat
	ln -sf /usr/include/libdrm include_compat/libdrm

# Sources locales des compresseurs (copie du projet ELFZ)
COMPRESSOR_LZ4_SRCS = compressors/lz4/lz4.c compressors/lz4/lz4hc.c compressors/lz4/lz4_compress_wrapper.c
COMPRESSOR_ZX0_SRCS = compressors/zx0/compress.c compressors/zx0/memory.c compressors/zx0/optimize.c compressors/zx0/zx0_compress_wrapper.c
COMPRESSOR_INC = -Icompressors/lz4 -Icompressors/zx0

# Flags optimisation agressive pour bin_packer (outil host, pas embarqué)
PACKER_CFLAGS = -O3 -march=native -mtune=native -flto=auto
PGO_DIR = pgo_profiles

# Génère les données de profiling ZX0 (à faire UNE SEULE FOIS, ~25 min)
# Les profils sont sauvegardés dans pgo_profiles/ et réutilisés ensuite.
# À relancer uniquement si le code du compresseur ZX0 change.
pgo-train: cad_rescue_gui $(COMPRESSOR_LZ4_SRCS) $(COMPRESSOR_ZX0_SRCS) bin_packer.c
	@echo "=== PGO: Building instrumented bin_packer ==="
	gcc $(PACKER_CFLAGS) -fprofile-generate $(COMPRESSOR_INC) \
	    $(COMPRESSOR_LZ4_SRCS) $(COMPRESSOR_ZX0_SRCS) \
	    bin_packer.c -o bin_packer_pgo_train
	@echo "=== PGO: Profiling ZX0 on cad_rescue_gui (this takes a while...) ==="
	./bin_packer_pgo_train zx0 cad_rescue_gui /dev/null
	@mkdir -p $(PGO_DIR)
	@rm -f $(PGO_DIR)/*.gcda
	@for f in ./bin_packer_pgo_train-*.gcda; do \
		newf=$$(echo "$$f" | sed 's|.*/bin_packer_pgo_train-|$(PGO_DIR)/bin_packer-|'); \
		mv "$$f" "$$newf"; \
	done
	@rm -f bin_packer_pgo_train bin_packer_pgo_train-*.gcno
	@echo "=== PGO: Profils sauvegardés dans $(PGO_DIR)/ ==="
	@echo "=== Les prochains 'make bin_packer' utiliseront ces profils automatiquement. ==="

# bin_packer : utilise les profils PGO s'ils existent, sinon compile sans PGO
bin_packer: bin_packer.c $(COMPRESSOR_LZ4_SRCS) $(COMPRESSOR_ZX0_SRCS)
	@if [ -d $(PGO_DIR) ] && ls $(PGO_DIR)/*.gcda >/dev/null 2>&1; then \
		echo "=== bin_packer: Build avec PGO (profils trouvés dans $(PGO_DIR)/) ==="; \
		for f in $(PGO_DIR)/bin_packer-*.gcda; do \
			cp "$$f" .; \
		done; \
		gcc $(PACKER_CFLAGS) -fprofile-use -fprofile-correction $(COMPRESSOR_INC) \
		    $(COMPRESSOR_LZ4_SRCS) $(COMPRESSOR_ZX0_SRCS) \
		    bin_packer.c -o bin_packer; \
		rm -f ./bin_packer-*.gcda; \
	else \
		echo "=== bin_packer: Build sans PGO (pas de profils dans $(PGO_DIR)/) ==="; \
		echo "    Tip: 'make pgo-train' pour générer les profils (~25 min, une seule fois)"; \
		gcc $(PACKER_CFLAGS) $(COMPRESSOR_INC) \
		    $(COMPRESSOR_LZ4_SRCS) $(COMPRESSOR_ZX0_SRCS) \
		    bin_packer.c -o bin_packer; \
	fi

gui_payload.h: cad_rescue_gui bin_packer
	./bin_packer $(COMPRESS) cad_rescue_gui gui_payload.h

lnxcad: cad_watchdog.c $(DECOMPRESSOR_SRCS) gui_payload.h
	$(CC) $(CFLAGS) $(COMPRESS_FLAGS) -o $@ cad_watchdog.c $(DECOMPRESSOR_SRCS) $(LDFLAGS)

TRANSLATION_HEADERS = $(wildcard translations/*.h)

branding_icon.h:
	@if [ -f assets_build/branding.png ]; then \
		echo "Generating branding_icon.h from assets_build/branding.png..."; \
		python3 -c "import sys; from PIL import Image; img = Image.open('assets_build/branding.png').convert('RGBA'); width, height = img.size; f = open('branding_icon.h', 'w'); f.write('#define HAS_BRANDING 1\n#define BRANDING_W %d\n#define BRANDING_H %d\nstatic const uint32_t branding_data[%d] = {\n' % (width, height, width*height)); pixels = list(img.getdata()); [f.write('    ' + ''.join('0x%08X, ' % ((a << 24) | (r << 16) | (g << 8) | b) for r, g, b, a in pixels[y*width:(y+1)*width]) + '\n') for y in range(height)]; f.write('};\n'); f.close()"; \
	else \
		echo "No branding.png found. Generating dummy branding_icon.h..."; \
		echo "#define HAS_BRANDING 0" > branding_icon.h; \
	fi

theme_generator: theme_generator.c
	gcc theme_generator.c -o theme_generator

cad_theme.h: theme_generator
	./theme_generator

cursor_data.h custom_font.h: convert_pngs.py
	python3 convert_pngs.py

# Flags réduits pour libtsm (code tiers, on ne modifie pas leurs warnings)
TSM_WFLAGS = -Wno-unused-parameter -Wno-sign-compare -Wno-old-style-declaration

TSM_OBJS = $(TSM_SRCS:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) $(TSM_INC) $(DRM_INC) $(TSM_WFLAGS) -c -o $@ $<

cad_rescue_gui: cad_rescue_gui.c $(TSM_OBJS) $(COMPAT_DEP) $(TRANSLATION_HEADERS) branding_icon.h cad_theme.h cursor_data.h custom_font.h
	$(CC) $(CFLAGS) $(TSM_INC) $(DRM_INC) -o $@ cad_rescue_gui.c $(TSM_OBJS) $(LDFLAGS)

clean:
	rm -rf lnxcad cad_watchdog cad_rescue_gui bin_packer bin_packer_pgo_train gui_payload.h include_compat branding_icon.h theme_generator cad_theme.h cursor_data.h custom_font.h sleep_icon.h power_icon.h restart_icon.h shutdown_icon.h terminal_icon.h find_icon.h check1_icon.h check2_icon.h close_icon.h runcommand_icon.h console_icon.h taskmgr_icon.h password_icon.h
	find . -path "./$(PGO_DIR)" -prune -o \( -name '*.gcda' -o -name '*.gcno' \) -print | xargs rm -f 2>/dev/null || true
