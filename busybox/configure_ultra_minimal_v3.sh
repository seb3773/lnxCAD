#!/bin/bash
# Configuration ultra-minimale pour BusyBox rescue shell
# Seulement 27 applets essentiels

set -e

echo "=== Configuration BusyBox Ultra-Minimal ==="

# Liste des 27 applets essentiels
ESSENTIAL_APPLETS=(
    "ASH"           # Shell
    "LS"            # Lister fichiers
    "CAT"           # Afficher contenu
    "FIND"          # Rechercher fichiers
    "STAT"          # Info fichiers
    "DF"            # Espace disque
    "MOUNT"         # Monter FS
    "UMOUNT"        # Démonter FS
    "PS"            # Processus
    "KILL"          # Tuer processus
    "DMESG"         # Messages kernel
    "UNAME"         # Info système
    "GREP"          # Recherche texte
    "SED"           # Édition texte
    "AWK"           # Traitement texte
    "XARGS"         # Exécution commandes
    "HEXDUMP"       # Dump hexa
    "LOSETUP"       # Loop devices
    "BLKID"         # Identifier partitions
    "IP"            # Config réseau (préféré)
    "IFCONFIG"      # Config réseau (fallback)
    "PING"          # Test réseau
    "TAIL"          # Fin de fichier
    "RM"            # Supprimer
    "MV"            # Déplacer
    "ECHO"          # Afficher
    "CP"            # Copier
    "MKDIR"         # Créer répertoire
)

# Partir d'une config minimale
echo "Création d'une configuration minimale..."
make allnoconfig > /dev/null 2>&1

# Fonction pour forcer l'activation d'une option de configuration
enable_config() {
    local key=$1
    local val=${2:-y}
    # Supprimer toute définition existante du paramètre
    sed -i "/# $key is not set/d" .config
    sed -i "/^$key=/d" .config
    echo "$key=$val" >> .config
}

# Activer les options de base nécessaires
enable_config CONFIG_BUSYBOX
enable_config CONFIG_STATIC
enable_config CONFIG_LONG_OPTS
enable_config CONFIG_LFS
enable_config CONFIG_TIME64
enable_config CONFIG_FEATURE_SHOW_SCRIPT
enable_config CONFIG_FEATURE_VERSION
enable_config CONFIG_BUSYBOX_EXEC_PATH "\"/proc/self/exe\""

# Optimisations de taille
enable_config CONFIG_EXTRA_CFLAGS "\"-Os -ffunction-sections -fdata-sections\""
enable_config CONFIG_EXTRA_LDFLAGS "\"-Wl,--gc-sections -Wl,--strip-all\""

# Activer chaque applet essentiel
echo "Activation des applets essentiels..."
for applet in "${ESSENTIAL_APPLETS[@]}"; do
    echo "  - Activation de $applet"
    enable_config "CONFIG_${applet}"
done

# Activer les features nécessaires pour certains applets
enable_config CONFIG_FEATURE_SH_IS_ASH
enable_config CONFIG_ASH_OPTIMIZE_FOR_SIZE
enable_config CONFIG_ASH_INTERNAL_GLOB
enable_config CONFIG_ASH_BASH_COMPAT
enable_config CONFIG_ASH_JOB_CONTROL
enable_config CONFIG_ASH_ALIAS
enable_config CONFIG_ASH_GETOPTS
enable_config CONFIG_ASH_BUILTIN_ECHO
enable_config CONFIG_ASH_BUILTIN_PRINTF
enable_config CONFIG_ASH_BUILTIN_TEST
enable_config CONFIG_ASH_HELP
enable_config CONFIG_ASH_CMDCMD
enable_config CONFIG_ASH_EXPAND_PRMT
enable_config CONFIG_FEATURE_EDITING
enable_config CONFIG_FEATURE_EDITING_MAX_LEN 1024
enable_config CONFIG_FEATURE_EDITING_HISTORY 15

# Features pour ls
enable_config CONFIG_FEATURE_LS_SORTFILES
enable_config CONFIG_FEATURE_LS_TIMESTAMPS
enable_config CONFIG_FEATURE_LS_USERNAME
enable_config CONFIG_FEATURE_LS_COLOR

# Features pour grep
enable_config CONFIG_FEATURE_GREP_EGREP_ALIAS
enable_config CONFIG_FEATURE_GREP_FGREP_ALIAS
enable_config CONFIG_FEATURE_GREP_CONTEXT

# Features pour find
enable_config CONFIG_FEATURE_FIND_PRINT0
enable_config CONFIG_FEATURE_FIND_TYPE
enable_config CONFIG_FEATURE_FIND_PERM
enable_config CONFIG_FEATURE_FIND_MTIME
enable_config CONFIG_FEATURE_FIND_NEWER
enable_config CONFIG_FEATURE_FIND_EXEC
enable_config CONFIG_FEATURE_FIND_USER
enable_config CONFIG_FEATURE_FIND_GROUP
enable_config CONFIG_FEATURE_FIND_SIZE

# Features pour mount
enable_config CONFIG_FEATURE_MOUNT_LOOP
enable_config CONFIG_FEATURE_MOUNT_FLAGS
enable_config CONFIG_FEATURE_MOUNT_FSTAB

# Features pour df
enable_config CONFIG_FEATURE_DF_FANCY

# Features pour ip
enable_config CONFIG_FEATURE_IP_ADDRESS
enable_config CONFIG_FEATURE_IP_LINK
enable_config CONFIG_FEATURE_IP_ROUTE

# Valider la configuration
echo "Validation de la configuration..."
make oldconfig < /dev/null

echo ""
echo "=== Configuration terminée ==="
echo "Vérification des paramètres clés:"
grep "^CONFIG_STATIC=" .config
grep "^CONFIG_BUSYBOX=" .config
echo ""
echo "Nombre d'applets activés:"
grep "^CONFIG_[A-Z_]*=y" .config | grep -v "^CONFIG_FEATURE" | grep -v "^CONFIG_BUSYBOX" | grep -v "^CONFIG_LONG_OPTS" | grep -v "^CONFIG_LFS" | grep -v "^CONFIG_TIME64" | wc -l
echo ""
echo "Pour compiler: make clean && make -j\$(nproc)"

# Made with Bob
