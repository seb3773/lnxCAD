#!/bin/bash
# Compilation BusyBox statique ultra-optimisée avec gcc

set -e

echo "=== Compilation BusyBox statique optimisée ==="
echo ""

# Sauvegarder le binaire actuel
if [ -f busybox ]; then
    echo "Sauvegarde du binaire actuel..."
    cp busybox busybox.previous
    ls -lh busybox.previous
    echo ""
fi

# Nettoyer
echo "Nettoyage..."
make clean > /dev/null 2>&1

# Recharger la configuration
echo "Rechargement de la configuration..."
./configure_ultra_minimal_v3.sh > /dev/null 2>&1

# Compiler avec optimisations agressives
echo "Compilation avec optimisations agressives..."
echo "  CFLAGS: -Os -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables"
echo "  LDFLAGS: -static -Wl,--gc-sections -Wl,--strip-all"
echo ""

make -j$(nproc) \
    CFLAGS="-Os -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -fno-stack-protector" \
    LDFLAGS="-static -Wl,--gc-sections -Wl,--strip-all" 2>&1 | tail -10

echo ""
echo "=== Compilation terminée ==="
echo ""

# Strip agressif
echo "Strip agressif du binaire..."
strip --strip-all busybox
strip --remove-section=.comment busybox
strip --remove-section=.note busybox

# Optionnel: UPX compression (si disponible)
if command -v upx &> /dev/null; then
    echo "Compression UPX..."
    cp busybox busybox.uncompressed
    upx --best --lzma busybox 2>&1 | grep -E "(compressed|ratio)" || true
fi

# Comparaison des tailles
echo ""
echo "=== Comparaison des binaires ==="
if [ -f busybox.previous ]; then
    echo "Binaire précédent:"
    ls -lh busybox.previous | awk '{print "  Taille: " $5}'
fi
echo ""
echo "Binaire optimisé:"
ls -lh busybox | awk '{print "  Taille: " $5}'
file busybox | sed 's/^/  /'

# Vérifier que c'est bien statique
echo ""
echo "=== Vérification statique ==="
if ldd busybox 2>&1 | grep -q "not a dynamic executable"; then
    echo "✓ Binaire statique confirmé"
else
    echo "⚠ Attention: le binaire pourrait ne pas être complètement statique"
    ldd busybox 2>&1 | head -5
fi

# Lister les applets
echo ""
echo "=== Applets disponibles ==="
./busybox --list | wc -l | awk '{print "Total: " $1 " applets"}'

# Calcul de la réduction de taille
if [ -f busybox.previous ]; then
    echo ""
    echo "=== Gain de taille ==="
    SIZE_PREV=$(stat -c%s busybox.previous)
    SIZE_NEW=$(stat -c%s busybox)
    if [ $SIZE_NEW -lt $SIZE_PREV ]; then
        REDUCTION=$((SIZE_PREV - SIZE_NEW))
        PERCENT=$((REDUCTION * 100 / SIZE_PREV))
        echo "Réduction: $REDUCTION bytes ($PERCENT%)"
    else
        INCREASE=$((SIZE_NEW - SIZE_PREV))
        PERCENT=$((INCREASE * 100 / SIZE_PREV))
        echo "Augmentation: $INCREASE bytes ($PERCENT%)"
    fi
fi

echo ""
echo "=== Tests rapides ==="
./busybox echo "✓ echo fonctionne"
./busybox ls /tmp > /dev/null && echo "✓ ls fonctionne"
./busybox cat /proc/version > /dev/null && echo "✓ cat fonctionne"

echo ""
echo "Compilation optimisée terminée avec succès !"
echo ""
echo "Pour tester: ./busybox --list"

# Made with Bob
