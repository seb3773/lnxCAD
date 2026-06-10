#!/bin/sh
# build.sh - Theme switching and compilation wrapper for lnxCAD

set -e

THEME=${1:-base}
COMPRESS=${2:-lz4}

if [ ! -d "assets/$THEME" ]; then
    echo "Erreur : Thème '$THEME' inconnu."
    echo "Thèmes disponibles : base, color, tech"
    exit 1
fi

if [ "$COMPRESS" != "lz4" ] && [ "$COMPRESS" != "zx0" ]; then
    echo "Erreur : Mode de compression '$COMPRESS' inconnu."
    echo "Modes disponibles : lz4 (défaut), zx0"
    exit 1
fi

echo "=== Préparation du thème : $THEME (Compression : $COMPRESS) ==="
mkdir -p assets_build
rm -f assets_build/*
cp -r assets/$THEME/* assets_build/

# Supprimer les anciens fichiers générés pour forcer la regénération
rm -f cad_theme.h cursor_data.h custom_font.h branding_icon.h
rm -f sleep_icon.h power_icon.h restart_icon.h shutdown_icon.h terminal_icon.h
rm -f find_icon.h check1_icon.h check2_icon.h close_icon.h runcommand_icon.h
rm -f console_icon.h taskmgr_icon.h password_icon.h

echo "=== Génération des ressources du thème ==="
# Compiler et exécuter le générateur de thème
gcc theme_generator.c -o theme_generator
./theme_generator

# Exécuter le convertisseur Python pour les PNGs, la police TTF et le Xcursor
python3 convert_pngs.py

echo "=== Nettoyage des anciennes compilations ==="
make clean

echo "=== Compilation finale ==="
make COMPRESS=$COMPRESS
