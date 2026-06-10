#!/bin/bash
# Compilation BusyBox avec musl - approche correcte

set -e

echo "=== Compilation BusyBox avec musl (v2) ==="
echo ""

# Sauvegarder le binaire glibc actuel
if [ -f busybox ]; then
    echo "Sauvegarde du binaire glibc actuel..."
    cp busybox busybox.glibc
    ls -lh busybox.glibc
    echo ""
fi

# Nettoyer complètement
echo "Nettoyage complet..."
make clean > /dev/null 2>&1
make distclean > /dev/null 2>&1 || true

# Recharger la configuration
echo "Rechargement de la configuration..."
cp configs/lnxcad_rescue_defconfig .config
make oldconfig < /dev/null > /dev/null 2>&1


# La clé est d'utiliser musl-gcc SANS inclure les headers glibc
# musl-gcc est un wrapper qui pointe déjà vers les bons headers musl
echo "Compilation avec musl-gcc (sans headers glibc)..."
echo "  CC=musl-gcc"
echo "  CFLAGS=-Os -static"
echo "  LDFLAGS=-static"
echo ""

# Compiler en utilisant UNIQUEMENT musl
make -j$(nproc) \
    CC=musl-gcc \
    HOSTCC=gcc \
    CFLAGS="-Os -static" \
    LDFLAGS="-static" 2>&1 | tee /tmp/musl_build.log | tail -20

if [ ! -f busybox ]; then
    echo ""
    echo "❌ Erreur de compilation. Voir /tmp/musl_build.log pour les détails"
    echo ""
    echo "Dernières erreurs:"
    tail -30 /tmp/musl_build.log
    exit 1
fi

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
