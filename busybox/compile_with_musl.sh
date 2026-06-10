#!/bin/bash
# Compilation BusyBox avec musl pour un binaire statique ultra-léger

set -e

echo "=== Compilation BusyBox avec musl ==="
echo ""

# Sauvegarder le binaire glibc actuel
if [ -f busybox ]; then
    echo "Sauvegarde du binaire glibc actuel..."
    cp busybox busybox.glibc
    ls -lh busybox.glibc
    echo ""
fi

# Nettoyer
echo "Nettoyage..."
make clean > /dev/null 2>&1

# Compiler avec musl
echo "Compilation avec musl-gcc..."
echo "  CC=musl-gcc"
echo "  CFLAGS=-Os -pipe -static"
echo "  LDFLAGS=-static"
echo ""

make -j$(nproc) \
    CC=musl-gcc \
    CFLAGS="-Os -pipe -static -I/usr/include -I/usr/include/x86_64-linux-gnu" \
    LDFLAGS="-static" 2>&1 | tail -10

echo ""
echo "=== Compilation terminée ==="
echo ""

# Strip le binaire
echo "Strip du binaire..."
strip --strip-all busybox

# Comparaison des tailles
echo ""
echo "=== Comparaison des binaires ==="
if [ -f busybox.glibc ]; then
    echo "Binaire glibc:"
    ls -lh busybox.glibc | awk '{print "  Taille: " $5}'
    file busybox.glibc | sed 's/^/  /'
fi
echo ""
echo "Binaire musl:"
ls -lh busybox | awk '{print "  Taille: " $5}'
file busybox | sed 's/^/  /'

# Vérifier que c'est bien statique
echo ""
echo "=== Vérification statique ==="
if ldd busybox 2>&1 | grep -q "not a dynamic executable"; then
    echo "✓ Binaire statique confirmé (musl)"
else
    echo "⚠ Attention: le binaire pourrait ne pas être complètement statique"
    ldd busybox 2>&1 | head -5
fi

# Lister les applets
echo ""
echo "=== Applets disponibles ==="
./busybox --list | wc -l | awk '{print "Total: " $1 " applets"}'

# Calcul de la réduction de taille
if [ -f busybox.glibc ]; then
    echo ""
    echo "=== Gain de taille ==="
    SIZE_GLIBC=$(stat -c%s busybox.glibc)
    SIZE_MUSL=$(stat -c%s busybox)
    REDUCTION=$((SIZE_GLIBC - SIZE_MUSL))
    PERCENT=$((REDUCTION * 100 / SIZE_GLIBC))
    echo "Réduction: $REDUCTION bytes ($PERCENT%)"
fi

echo ""
echo "=== Tests rapides ==="
./busybox echo "✓ echo fonctionne"
./busybox ls /tmp > /dev/null && echo "✓ ls fonctionne"
./busybox cat /proc/version > /dev/null && echo "✓ cat fonctionne"

echo ""
echo "Compilation avec musl terminée avec succès !"

# Made with Bob
