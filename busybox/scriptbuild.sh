#!/usr/bin/env bash
set -euo pipefail

# Sauvegarde
cp -v .config .config.bak 2>/dev/null || true

# 1) Assure-toi d'avoir une base valide
rm -f .config.old 2>/dev/null || true
if ! grep -q '^CONFIG_HAVE_DOT_CONFIG=' .config 2>/dev/null; then
  echo "Génération d'une base valide (allnoconfig)..."
  make allnoconfig
fi

# 2) Construire la liste des symboles Kconfig valides (Config.in et inclus)
grep -oP '^\s*config\s+\K[A-Z0-9_]+' $(find . -name 'Config.in' -print) | sort -u > 
/tmp/bb_valid_symbols.txt

# 3) Extraire la liste d'applets définies dans le code source (BUSYBOX_APPLET / applet 
# definitions)
#    On recherche les macros BUSYBOX_APPLET et les prototypes d'applet pour récupérer le 
#    nom "cp", "mv", etc.
grep -R --line-number -E 'BUSYBOX_APPLET|applet\(|APPUTIL' . 2>/dev/null | sed -n 
'1,20000p' > /tmp/bb_applets_raw.txt || true

# 4) Extraire noms d'applets simples (heuristique fiable : chercher "BUSYBOX_APPLET(name" 
# ou "applet(name")
#    On normalise en majuscules pour construire CONFIG_<NAME>
grep -oP 'BUSYBOX_APPLET\(\s*\K[a-z0-9_]+' -n $(grep -lR 'BUSYBOX_APPLET' . 2>/dev/null) 
2>/dev/null | sed 's/:.*//' | sort -u > /tmp/bb_applets_list.txt || true
# fallback : chercher "applet(" occurrences
if [ ! -s /tmp/bb_applets_list.txt ]; then
  grep -oP 'applet\(\s*"\K[a-z0-9_]+' -R 2>/dev/null | sort -u > /tmp/bb_applets_list.txt 
|| true
fi

# 5) Si tu veux une liste restreinte, modifie WANT_APPLETS ci-dessous (ex: WANT="cp mv find 
# dd hexdump")
#    Sinon, on prend une sélection raisonnable pour rescue industriel
WANT="cp mv rm find tar dd hexdump blkid losetup grep sed awk xargs ifconfig ip ping stat 
df mkdir rmdir mount umount ps kill top"

# 6) Pour chaque applet demandé, on cherche s'il existe dans la liste extraite et on active 
# le symbole Kconfig correspondant
echo "Activation automatique des applets demandés (si présents dans la source / 
Kconfig)..."
for a in $WANT; do
  # normaliser nom de symbole : CP -> CONFIG_CP
  sym_upper=$(echo "$a" | tr '[:lower:]' '[:upper:]' | sed 's/-/_/g')
  ksym="CONFIG_${sym_upper}"

  # heuristique : certains applets ont noms Kconfig différents (ex: hexdump -> 
  # CONFIG_HEXDUMP, dd -> CONFIG_DD)
  # vérifier si le symbole existe dans la liste valide
  if grep -qx "$sym_upper" /tmp/bb_applets_list.txt 2>/dev/null || grep -q "^${ksym}=" 
.config 2>/dev/null || grep -q "^# ${ksym} is not set" .config 2>/dev/null; then
    # n'activer que si le symbole est présent dans les Config.in valides
    if grep -qx "${sym_upper}" /tmp/bb_valid_symbols.txt || grep -q "^${ksym}=" .config || 
grep -q "^# ${ksym} is not set" .config; then
      if grep -q "^# ${ksym} is not set" .config 2>/dev/null; then
        sed -i "s/^# ${ksym} is not set/${ksym}=y/" .config
      elif grep -q "^${ksym}=" .config 2>/dev/null; then
        sed -i "s/^${ksym}=.*/${ksym}=y/" .config
      else
        echo "${ksym}=y" >> .config
      fi
      echo "Enabled: ${ksym}"
    else
      echo "Symbol Kconfig absent pour ${a} (pas activé)"
    fi
  else
    # si l'applet n'est pas trouvée dans la liste d'applets, on tente une recherche 
    # textuelle dans les sources pour trouver le symbole Kconfig
    # recherche du mot 'config .*<NAME>' dans tous les Config.in
    found=$(grep -R --line-number -E "config\s+[A-Z0-9_]*${sym_upper}[A-Z0-9_]*" $(find . 
-name 'Config.in' -print) 2>/dev/null || true)
    if [ -n "$found" ]; then
      # extraire le nom exact du symbole
      k=$(echo "$found" | head -n1 | sed -n '1p' | sed -E 
's/.*config\s+([A-Z0-9_]+).*/\1/')
      if [ -n "$k" ]; then
        if grep -q "^# CONFIG_${k} is not set" .config 2>/dev/null; then
          sed -i "s/^# CONFIG_${k} is not set/CONFIG_${k}=y/" .config
        elif grep -q "^CONFIG_${k}=" .config 2>/dev/null; then
          sed -i "s/^CONFIG_${k}=.*/CONFIG_${k}=y/" .config
        else
          echo "CONFIG_${k}=y" >> .config
        fi
        echo "Enabled via heuristic: CONFIG_${k} (for applet ${a})"
      else
        echo "Impossible de trouver symbole Kconfig pour ${a}"
      fi
    else
      echo "Applet introuvable dans source: ${a}"
    fi
  fi
done

# 7) Forcer désactivation des fonctions d'installation/écriture automatiques
sed -i 's/^CONFIG_FEATURE_INSTALLER=.*/CONFIG_FEATURE_INSTALLER=n/' .config 2>/dev/null || 
true
sed -i 's/^# CONFIG_FEATURE_INSTALLER is not set/CONFIG_FEATURE_INSTALLER=n/' .config 
2>/dev/null || true
sed -i 's/^CONFIG_INSTALL_APPLET_SYMLINKS=.*/CONFIG_INSTALL_APPLET_SYMLINKS=n/' .config 
2>/dev/null || true
grep -q '^CONFIG_INSTALL_APPLET_DONT=y' .config || echo 'CONFIG_INSTALL_APPLET_DONT=y' >> 
.config
sed -i 's/^CONFIG_FEATURE_PIDFILE=.*/CONFIG_FEATURE_PIDFILE=n/' .config 2>/dev/null || true
sed -i 's/^CONFIG_FEATURE_SYSLOG=.*/CONFIG_FEATURE_SYSLOG=n/' .config 2>/dev/null || true
sed -i 's/^CONFIG_FEATURE_SUID=.*/CONFIG_FEATURE_SUID=n/' .config 2>/dev/null || true

# 8) Nettoyage et vérif
sed -i 's/^[[:space:]]*//' .config
sed -i '/^[[:space:]]*$/d' .config
echo "Extrait .config (20 premières lignes) :"
head -n 20 .config

# 9) Compile non interactif
echo "Compilation..."
make -j"$(nproc)"
echo "Terminé. Vérifie ./busybox --list"


