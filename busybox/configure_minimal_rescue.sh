#!/bin/bash
# Configuration minimale pour BusyBox rescue shell
# Active uniquement les outils essentiels

set -e

echo "Configuration d'un BusyBox minimal pour rescue shell..."

# Activer les options de base
sed -i 's/^# CONFIG_BUSYBOX is not set/CONFIG_BUSYBOX=y/' .config
sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
sed -i 's/^# CONFIG_LFS is not set/CONFIG_LFS=y/' .config
sed -i 's/^# CONFIG_LONG_OPTS is not set/CONFIG_LONG_OPTS=y/' .config
sed -i 's/^# CONFIG_FEATURE_EDITING is not set/CONFIG_FEATURE_EDITING=y/' .config

# Désactiver les messages d'aide
sed -i 's/^CONFIG_SHOW_USAGE=.*/# CONFIG_SHOW_USAGE is not set/' .config
sed -i 's/^CONFIG_FEATURE_VERBOSE_USAGE=.*/# CONFIG_FEATURE_VERBOSE_USAGE is not set/' .config

# Optimisations de compilation
sed -i 's|^CONFIG_EXTRA_CFLAGS=.*|CONFIG_EXTRA_CFLAGS="-Os -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables"|' .config
sed -i 's|^CONFIG_EXTRA_LDFLAGS=.*|CONFIG_EXTRA_LDFLAGS="-Wl,--gc-sections -Wl,--strip-all"|' .config

# Shell (ASH minimal)
sed -i 's/^# CONFIG_ASH is not set/CONFIG_ASH=y/' .config
sed -i 's/^# CONFIG_ASH_OPTIMIZE_FOR_SIZE is not set/CONFIG_ASH_OPTIMIZE_FOR_SIZE=y/' .config
sed -i 's/^# CONFIG_ASH_INTERNAL_GLOB is not set/CONFIG_ASH_INTERNAL_GLOB=y/' .config
sed -i 's/^# CONFIG_ASH_JOB_CONTROL is not set/CONFIG_ASH_JOB_CONTROL=y/' .config
sed -i 's/^# CONFIG_ASH_ECHO is not set/CONFIG_ASH_ECHO=y/' .config
sed -i 's/^# CONFIG_ASH_PRINTF is not set/CONFIG_ASH_PRINTF=y/' .config
sed -i 's/^# CONFIG_ASH_TEST is not set/CONFIG_ASH_TEST=y/' .config
sed -i 's/^# CONFIG_FEATURE_SH_MATH is not set/CONFIG_FEATURE_SH_MATH=y/' .config

# Coreutils essentiels (lecture seule)
sed -i 's/^# CONFIG_LS is not set/CONFIG_LS=y/' .config
sed -i 's/^# CONFIG_CAT is not set/CONFIG_CAT=y/' .config
sed -i 's/^# CONFIG_ECHO is not set/CONFIG_ECHO=y/' .config
sed -i 's/^# CONFIG_PRINTF is not set/CONFIG_PRINTF=y/' .config
sed -i 's/^# CONFIG_TEST is not set/CONFIG_TEST=y/' .config
sed -i 's/^# CONFIG_TRUE is not set/CONFIG_TRUE=y/' .config
sed -i 's/^# CONFIG_FALSE is not set/CONFIG_FALSE=y/' .config
sed -i 's/^# CONFIG_PWD is not set/CONFIG_PWD=y/' .config
sed -i 's/^# CONFIG_BASENAME is not set/CONFIG_BASENAME=y/' .config
sed -i 's/^# CONFIG_DIRNAME is not set/CONFIG_DIRNAME=y/' .config
sed -i 's/^# CONFIG_HEAD is not set/CONFIG_HEAD=y/' .config
sed -i 's/^# CONFIG_TAIL is not set/CONFIG_TAIL=y/' .config
sed -i 's/^# CONFIG_WC is not set/CONFIG_WC=y/' .config
sed -i 's/^# CONFIG_SORT is not set/CONFIG_SORT=y/' .config
sed -i 's/^# CONFIG_UNIQ is not set/CONFIG_UNIQ=y/' .config
sed -i 's/^# CONFIG_CUT is not set/CONFIG_CUT=y/' .config
sed -i 's/^# CONFIG_TR is not set/CONFIG_TR=y/' .config
sed -i 's/^# CONFIG_STAT is not set/CONFIG_STAT=y/' .config
sed -i 's/^# CONFIG_DF is not set/CONFIG_DF=y/' .config
sed -i 's/^# CONFIG_DU is not set/CONFIG_DU=y/' .config
sed -i 's/^# CONFIG_READLINK is not set/CONFIG_READLINK=y/' .config

# Outils de recherche
sed -i 's/^# CONFIG_FIND is not set/CONFIG_FIND=y/' .config
sed -i 's/^# CONFIG_GREP is not set/CONFIG_GREP=y/' .config
sed -i 's/^# CONFIG_EGREP is not set/CONFIG_EGREP=y/' .config
sed -i 's/^# CONFIG_FGREP is not set/CONFIG_FGREP=y/' .config
sed -i 's/^# CONFIG_XARGS is not set/CONFIG_XARGS=y/' .config

# Éditeurs de texte
sed -i 's/^# CONFIG_SED is not set/CONFIG_SED=y/' .config
sed -i 's/^# CONFIG_AWK is not set/CONFIG_AWK=y/' .config

# Système de fichiers
sed -i 's/^# CONFIG_MOUNT is not set/CONFIG_MOUNT=y/' .config
sed -i 's/^# CONFIG_UMOUNT is not set/CONFIG_UMOUNT=y/' .config
sed -i 's/^# CONFIG_BLKID is not set/CONFIG_BLKID=y/' .config
sed -i 's/^# CONFIG_LOSETUP is not set/CONFIG_LOSETUP=y/' .config

# Process management
sed -i 's/^# CONFIG_PS is not set/CONFIG_PS=y/' .config
sed -i 's/^# CONFIG_KILL is not set/CONFIG_KILL=y/' .config
sed -i 's/^# CONFIG_PIDOF is not set/CONFIG_PIDOF=y/' .config

# Diagnostic système
sed -i 's/^# CONFIG_DMESG is not set/CONFIG_DMESG=y/' .config
sed -i 's/^# CONFIG_UNAME is not set/CONFIG_UNAME=y/' .config

# Inspection binaire
sed -i 's/^# CONFIG_HEXDUMP is not set/CONFIG_HEXDUMP=y/' .config
sed -i 's/^# CONFIG_HD is not set/CONFIG_HD=y/' .config
sed -i 's/^# CONFIG_STRINGS is not set/CONFIG_STRINGS=y/' .config

# Réseau (optionnel - diagnostic uniquement)
sed -i 's/^# CONFIG_IFCONFIG is not set/CONFIG_IFCONFIG=y/' .config
sed -i 's/^# CONFIG_IP is not set/CONFIG_IP=y/' .config
sed -i 's/^# CONFIG_PING is not set/CONFIG_PING=y/' .config
sed -i 's/^# CONFIG_HOSTNAME is not set/CONFIG_HOSTNAME=y/' .config

# Outils d'intervention (écriture) - OPTIONNELS
# Décommenter si nécessaire pour rescue complet
sed -i 's/^# CONFIG_CP is not set/CONFIG_CP=y/' .config
sed -i 's/^# CONFIG_MV is not set/CONFIG_MV=y/' .config
sed -i 's/^# CONFIG_RM is not set/CONFIG_RM=y/' .config
sed -i 's/^# CONFIG_DD is not set/CONFIG_DD=y/' .config
sed -i 's/^# CONFIG_TAR is not set/CONFIG_TAR=y/' .config
sed -i 's/^# CONFIG_GZIP is not set/CONFIG_GZIP=y/' .config
sed -i 's/^# CONFIG_GUNZIP is not set/CONFIG_GUNZIP=y/' .config
sed -i 's/^# CONFIG_MKDIR is not set/CONFIG_MKDIR=y/' .config
sed -i 's/^# CONFIG_RMDIR is not set/CONFIG_RMDIR=y/' .config
sed -i 's/^# CONFIG_TOUCH is not set/CONFIG_TOUCH=y/' .config
sed -i 's/^# CONFIG_LN is not set/CONFIG_LN=y/' .config
sed -i 's/^# CONFIG_CHMOD is not set/CONFIG_CHMOD=y/' .config
sed -i 's/^# CONFIG_CHOWN is not set/CONFIG_CHOWN=y/' .config

echo "Configuration terminée. Lancement de 'make oldconfig' pour valider..."
yes "" | make oldconfig > /dev/null 2>&1

echo "Configuration minimale appliquée avec succès!"
echo "Nombre d'applets activés:"
grep "^CONFIG_.*=y$" .config | grep -v "^CONFIG_HAVE_DOT_CONFIG" | grep -v "^CONFIG_FEATURE_" | grep -v "^CONFIG_INSTALL_" | wc -l

# Made with Bob
