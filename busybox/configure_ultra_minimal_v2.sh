#!/bin/bash
# Configuration ULTRA minimale pour BusyBox rescue shell
# Modification directe du .config pour garder uniquement 27 applets

set -e

echo "Désactivation de tous les applets non essentiels..."

# Liste des applets à GARDER (27 applets)
KEEP_APPLETS=(
    "CONFIG_ASH"
    "CONFIG_LS"
    "CONFIG_CAT"
    "CONFIG_FIND"
    "CONFIG_STAT"
    "CONFIG_DF"
    "CONFIG_MOUNT"
    "CONFIG_UMOUNT"
    "CONFIG_PS"
    "CONFIG_KILL"
    "CONFIG_DMESG"
    "CONFIG_UNAME"
    "CONFIG_GREP"
    "CONFIG_EGREP"
    "CONFIG_FGREP"
    "CONFIG_SED"
    "CONFIG_AWK"
    "CONFIG_XARGS"
    "CONFIG_HEXDUMP"
    "CONFIG_HD"
    "CONFIG_LOSETUP"
    "CONFIG_BLKID"
    "CONFIG_IP"
    "CONFIG_IFCONFIG"
    "CONFIG_PING"
    "CONFIG_TAIL"
    "CONFIG_RM"
    "CONFIG_MV"
    "CONFIG_ECHO"
    "CONFIG_CP"
    "CONFIG_MKDIR"
)

# Désactiver TOUS les applets d'abord
sed -i 's/^CONFIG_[A-Z_]*=y$/# & is not set/' .config

# Réactiver les applets essentiels
for applet in "${KEEP_APPLETS[@]}"; do
    sed -i "s/^# ${applet}=y is not set/${applet}=y/" .config
    sed -i "s/^# ${applet} is not set/${applet}=y/" .config
done

# S'assurer que les options de base sont activées
sed -i 's/^# CONFIG_BUSYBOX is not set/CONFIG_BUSYBOX=y/' .config
sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
sed -i 's/^# CONFIG_LFS is not set/CONFIG_LFS=y/' .config
sed -i 's/^# CONFIG_FEATURE_EDITING is not set/CONFIG_FEATURE_EDITING=y/' .config

# Activer les sous-features nécessaires pour les applets gardés
sed -i 's/^# CONFIG_FEATURE_LS_FILETYPES is not set/CONFIG_FEATURE_LS_FILETYPES=y/' .config
sed -i 's/^# CONFIG_FEATURE_LS_SORTFILES is not set/CONFIG_FEATURE_LS_SORTFILES=y/' .config
sed -i 's/^# CONFIG_FEATURE_FANCY_ECHO is not set/CONFIG_FEATURE_FANCY_ECHO=y/' .config
sed -i 's/^# CONFIG_FEATURE_FANCY_TAIL is not set/CONFIG_FEATURE_FANCY_TAIL=y/' .config
sed -i 's/^# CONFIG_FEATURE_GREP_CONTEXT is not set/CONFIG_FEATURE_GREP_CONTEXT=y/' .config
sed -i 's/^# CONFIG_FEATURE_MOUNT_LOOP is not set/CONFIG_FEATURE_MOUNT_LOOP=y/' .config
sed -i 's/^# CONFIG_FEATURE_BLKID_TYPE is not set/CONFIG_FEATURE_BLKID_TYPE=y/' .config
sed -i 's/^# CONFIG_FEATURE_IP_ADDRESS is not set/CONFIG_FEATURE_IP_ADDRESS=y/' .config
sed -i 's/^# CONFIG_FEATURE_IP_LINK is not set/CONFIG_FEATURE_IP_LINK=y/' .config
sed -i 's/^# CONFIG_FEATURE_IP_ROUTE is not set/CONFIG_FEATURE_IP_ROUTE=y/' .config
sed -i 's/^# CONFIG_FEATURE_IFCONFIG_STATUS is not set/CONFIG_FEATURE_IFCONFIG_STATUS=y/' .config
sed -i 's/^# CONFIG_FEATURE_FANCY_PING is not set/CONFIG_FEATURE_FANCY_PING=y/' .config
sed -i 's/^# CONFIG_FEATURE_AWK_LIBM is not set/CONFIG_FEATURE_AWK_LIBM=y/' .config
sed -i 's/^# CONFIG_FEATURE_SH_MATH is not set/CONFIG_FEATURE_SH_MATH=y/' .config

# Activer les sous-applets IP
sed -i 's/^# CONFIG_IPADDR is not set/CONFIG_IPADDR=y/' .config
sed -i 's/^# CONFIG_IPLINK is not set/CONFIG_IPLINK=y/' .config
sed -i 's/^# CONFIG_IPROUTE is not set/CONFIG_IPROUTE=y/' .config
sed -i 's/^# CONFIG_IPTUNNEL is not set/CONFIG_IPTUNNEL=y/' .config
sed -i 's/^# CONFIG_IPRULE is not set/CONFIG_IPRULE=y/' .config
sed -i 's/^# CONFIG_IPNEIGH is not set/CONFIG_IPNEIGH=y/' .config

# Shell necessities
sed -i 's/^# CONFIG_SH_IS_ASH is not set/CONFIG_SH_IS_ASH=y/' .config
sed -i 's/^# CONFIG_ASH_OPTIMIZE_FOR_SIZE is not set/CONFIG_ASH_OPTIMIZE_FOR_SIZE=y/' .config
sed -i 's/^# CONFIG_ASH_INTERNAL_GLOB is not set/CONFIG_ASH_INTERNAL_GLOB=y/' .config
sed -i 's/^# CONFIG_ASH_JOB_CONTROL is not set/CONFIG_ASH_JOB_CONTROL=y/' .config
sed -i 's/^# CONFIG_ASH_ECHO is not set/CONFIG_ASH_ECHO=y/' .config
sed -i 's/^# CONFIG_ASH_PRINTF is not set/CONFIG_ASH_PRINTF=y/' .config
sed -i 's/^# CONFIG_ASH_TEST is not set/CONFIG_ASH_TEST=y/' .config

# Builtins nécessaires
sed -i 's/^# CONFIG_TEST is not set/CONFIG_TEST=y/' .config
sed -i 's/^# CONFIG_TRUE is not set/CONFIG_TRUE=y/' .config
sed -i 's/^# CONFIG_FALSE is not set/CONFIG_FALSE=y/' .config
sed -i 's/^# CONFIG_PRINTF is not set/CONFIG_PRINTF=y/' .config

echo "Validation avec oldconfig..."
yes "" | make oldconfig > /dev/null 2>&1

echo "Configuration appliquée!"
echo "Applets configurés:"
grep "^CONFIG_.*=y$" .config | grep -E "^CONFIG_(ASH|LS|CAT|FIND|STAT|DF|MOUNT|UMOUNT|PS|KILL|DMESG|UNAME|GREP|SED|AWK|XARGS|HEXDUMP|HD|LOSETUP|BLKID|IP|IFCONFIG|PING|TAIL|RM|MV|ECHO|CP|MKDIR|EGREP|FGREP|TEST|TRUE|FALSE|PRINTF)=" | wc -l

# Made with Bob
