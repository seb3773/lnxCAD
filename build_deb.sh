#!/bin/sh
# build_deb.sh - Build Debian package for lnxCAD

set -e

PACKAGE_NAME="lnxcad"
VERSION="1.0"
ARCH="amd64"
BUILD_DIR="${PACKAGE_NAME}_${VERSION}_${ARCH}"

echo "=== Preparing Debian Package Build Directory ==="
# Clean old build directory
rm -rf "$BUILD_DIR"
rm -f "${BUILD_DIR}.deb"

# Find binary
SRC_BIN=""
if [ -f "./lnxcad" ]; then
    SRC_BIN="./lnxcad"
elif [ -f "./precompiled_binairies/base/lnxcad" ]; then
    SRC_BIN="./precompiled_binairies/base/lnxcad"
fi

if [ -z "$SRC_BIN" ]; then
    echo "Error: 'lnxcad' binary not found. Please compile it first using ./build.sh." >&2
    exit 1
fi

echo "Using binary: $SRC_BIN"

# Create directories
mkdir -p "$BUILD_DIR/DEBIAN"
mkdir -p "$BUILD_DIR/usr/sbin"

# Copy binary to destination in package
cp "$SRC_BIN" "$BUILD_DIR/usr/sbin/lnxcad"
chmod 755 "$BUILD_DIR/usr/sbin/lnxcad"

# Generate control file
cat << EOF > "$BUILD_DIR/DEBIAN/control"
Package: $PACKAGE_NAME
Version: $VERSION
Section: admin
Priority: optional
Architecture: $ARCH
Maintainer: seb3773 <https://github.com/seb3773>
Description: Ctrl+Alt+Del Rescue GUI Watchdog
 Monitors keyboard for Ctrl+Alt+Del and launches a
 DRM-based rescue interface with shutdown, reboot,
 terminal, and task management capabilities.
EOF

# Generate postinst script
cat << 'EOF' > "$BUILD_DIR/DEBIAN/postinst"
#!/bin/sh
set -e

BIN_DEST="/usr/sbin/lnxcad"

if [ "$1" = "configure" ]; then
    detect_init() {
        p1_comm=""
        if [ -f /proc/1/comm ]; then
            p1_comm=$(cat /proc/1/comm)
        fi

        if [ "$p1_comm" = "systemd" ] || [ -d /run/systemd/system ]; then
            echo "systemd"
            return
        fi

        if [ "$p1_comm" = "runit" ] || [ "$p1_comm" = "runsvdir" ] || [ -d /run/runit ]; then
            echo "runit"
            return
        fi

        if which rc-status >/dev/null 2>&1; then
            echo "openrc"
            return
        fi

        if [ -d /etc/init.d ]; then
            if [ -f /etc/inittab ]; then
                echo "sysvinit_inittab"
            else
                echo "sysvinit"
            fi
            return
        fi

        echo "fallback"
    }

    INIT_SYSTEM=$(detect_init)
    echo "lnxCAD: configuring startup for $INIT_SYSTEM..."

    case "$INIT_SYSTEM" in
        systemd)
            cat << 'INNER_EOF' > /etc/systemd/system/lnxcad.service
[Unit]
Description=lnxCAD - Ctrl+Alt+Del Rescue Watchdog
Documentation=https://github.com/seb3773/lnxCAD
After=systemd-logind.service

[Service]
Type=simple
ExecStart=/usr/sbin/lnxcad
Restart=always
RestartSec=2
OOMScoreAdjust=-1000
Nice=-20
SupplementaryGroups=input

[Install]
WantedBy=multi-user.target
INNER_EOF
            chmod 644 /etc/systemd/system/lnxcad.service
            systemctl daemon-reload || true
            systemctl enable lnxcad.service || true
            systemctl start lnxcad.service || true
            ;;
            
        sysvinit_inittab)
            if ! grep -q "^cad:2345:respawn:$BIN_DEST" /etc/inittab; then
                echo "" >> /etc/inittab
                echo "# lnxCAD Rescue Watchdog" >> /etc/inittab
                echo "cad:2345:respawn:$BIN_DEST" >> /etc/inittab
            fi
            if which telinit >/dev/null 2>&1; then
                telinit q || true
            elif which init >/dev/null 2>&1; then
                init q || true
            fi
            if ! pgrep -f "$BIN_DEST" >/dev/null 2>&1; then
                "$BIN_DEST" &
            fi
            ;;
            
        sysvinit|openrc)
            cat << 'INNER_EOF' > /etc/init.d/lnxcad
#!/bin/sh
### BEGIN INIT INFO
# Provides:          lnxcad
# Required-Start:    $local_fs $syslog
# Required-Stop:     $local_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: lnxCAD Rescue Watchdog
# Description:       Monitors Ctrl+Alt+Del and launches rescue GUI
### END INIT INFO

DAEMON=/usr/sbin/lnxcad
NAME=lnxcad
PIDFILE=/var/run/lnxcad.pid

if ! which start-stop-daemon >/dev/null 2>&1; then
    start_daemon() {
        "$DAEMON" &
        echo $! > "$PIDFILE"
    }
    stop_daemon() {
        if [ -f "$PIDFILE" ]; then
            pid=$(cat "$PIDFILE")
            kill "$pid" 2>/dev/null
            sleep 1
            kill -9 "$pid" 2>/dev/null
            rm -f "$PIDFILE"
        fi
    }
else
    start_daemon() {
        start-stop-daemon --start --background --make-pidfile --pidfile "$PIDFILE" --exec "$DAEMON"
    }
    stop_daemon() {
        start-stop-daemon --stop --pidfile "$PIDFILE" --retry=TERM/10/KILL/5
        rm -f "$PIDFILE"
    }
fi

case "$1" in
  start)
    if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        exit 0
    fi
    start_daemon
    ;;
  stop)
    stop_daemon
    ;;
  restart)
    $0 stop
    sleep 1
    $0 start
    ;;
  status)
    if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
      exit 0
    else
      exit 3
    fi
    ;;
esac
INNER_EOF
            chmod 755 /etc/init.d/lnxcad
            
            if which rc-update >/dev/null 2>&1; then
                rc-update add lnxcad default || true
                rc-service lnxcad start || true
            elif which update-rc.d >/dev/null 2>&1; then
                update-rc.d lnxcad defaults || true
                update-rc.d lnxcad enable || true
                invoke-rc.d lnxcad start || /etc/init.d/lnxcad start || true
            elif which chkconfig >/dev/null 2>&1; then
                chkconfig --add lnxcad || true
                chkconfig lnxcad on || true
                service lnxcad start || /etc/init.d/lnxcad start || true
            else
                /etc/init.d/lnxcad start || true
            fi
            ;;
            
        runit)
            mkdir -p /etc/sv/lnxcad
            cat << 'INNER_EOF' > /etc/sv/lnxcad/run
#!/bin/sh
exec /usr/sbin/lnxcad
INNER_EOF
            chmod 755 /etc/sv/lnxcad/run
            
            service_dir=""
            if [ -d /var/service ]; then
                service_dir="/var/service"
            elif [ -d /etc/service ]; then
                service_dir="/etc/service"
            fi
            
            if [ -n "$service_dir" ]; then
                ln -sf /etc/sv/lnxcad "$service_dir/lnxcad"
                which sv >/dev/null 2>&1 && sv start lnxcad || true
            fi
            ;;
            
        fallback)
            if [ -f /etc/rc.local ]; then
                if ! grep -q "$BIN_DEST" /etc/rc.local; then
                    if grep -q "^exit 0" /etc/rc.local; then
                        sed -i "s/^exit 0/$BIN_DEST \&\nexit 0/" /etc/rc.local
                    else
                        echo "$BIN_DEST &" >> /etc/rc.local
                    fi
                    chmod +x /etc/rc.local
                fi
            else
                cat << INNER_EOF > /etc/rc.local
#!/bin/sh -e
$BIN_DEST &
exit 0
INNER_EOF
                chmod +x /etc/rc.local
            fi
            if ! pgrep -f "$BIN_DEST" >/dev/null 2>&1; then
                "$BIN_DEST" &
            fi
            ;;
    esac
fi
EOF

# Generate prerm script
cat << 'EOF' > "$BUILD_DIR/DEBIAN/prerm"
#!/bin/sh
set -e

BIN_DEST="/usr/sbin/lnxcad"

if [ "$1" = "remove" ] || [ "$1" = "deconfigure" ]; then
    detect_init() {
        p1_comm=""
        if [ -f /proc/1/comm ]; then
            p1_comm=$(cat /proc/1/comm)
        fi

        if [ "$p1_comm" = "systemd" ] || [ -d /run/systemd/system ]; then
            echo "systemd"
            return
        fi

        if [ "$p1_comm" = "runit" ] || [ "$p1_comm" = "runsvdir" ] || [ -d /run/runit ]; then
            echo "runit"
            return
        fi

        if which rc-status >/dev/null 2>&1; then
            echo "openrc"
            return
        fi

        if [ -d /etc/init.d ]; then
            if [ -f /etc/inittab ]; then
                echo "sysvinit_inittab"
            else
                echo "sysvinit"
            fi
            return
        fi

        echo "fallback"
    }

    INIT_SYSTEM=$(detect_init)
    echo "lnxCAD: stopping services before removal..."

    case "$INIT_SYSTEM" in
        systemd)
            if systemctl is-active --quiet lnxcad.service 2>/dev/null; then
                systemctl stop lnxcad.service || true
            fi
            systemctl disable lnxcad.service 2>/dev/null || true
            ;;
            
        sysvinit_inittab)
            if [ -f /etc/inittab ]; then
                sed -i "\@^cad:2345:respawn:$BIN_DEST@d" /etc/inittab
                sed -i '/# lnxCAD Rescue Watchdog/d' /etc/inittab
                if [ -s /etc/inittab ]; then
                    sed -i -e :a -e '/^\n*$/{$d;N;ba}' /etc/inittab
                fi
                if which telinit >/dev/null 2>&1; then
                    telinit q || true
                elif which init >/dev/null 2>&1; then
                    init q || true
                fi
            fi
            pkill -f "$BIN_DEST" || true
            ;;
            
        sysvinit|openrc)
            if which rc-update >/dev/null 2>&1; then
                rc-service lnxcad stop 2>/dev/null || /etc/init.d/lnxcad stop 2>/dev/null || true
                rc-update del lnxcad default 2>/dev/null || true
            elif which update-rc.d >/dev/null 2>&1; then
                /etc/init.d/lnxcad stop 2>/dev/null || true
                update-rc.d -f lnxcad remove 2>/dev/null || true
            elif which chkconfig >/dev/null 2>&1; then
                /etc/init.d/lnxcad stop 2>/dev/null || true
                chkconfig --del lnxcad 2>/dev/null || true
            else
                /etc/init.d/lnxcad stop 2>/dev/null || true
            fi
            ;;
            
        runit)
            service_dir=""
            if [ -d /var/service ]; then
                service_dir="/var/service"
            elif [ -d /etc/service ]; then
                service_dir="/etc/service"
            fi
            
            if [ -n "$service_dir" ] && [ -L "$service_dir/lnxcad" ]; then
                rm -f "$service_dir/lnxcad"
            fi
            which sv >/dev/null 2>&1 && sv stop lnxcad 2>/dev/null || true
            ;;
            
        fallback)
            if [ -f /etc/rc.local ]; then
                sed -i "\@$BIN_DEST@d" /etc/rc.local
            fi
            pkill -f "$BIN_DEST" || true
            ;;
    esac

    pkill -f "$BIN_DEST" || true
fi
EOF

# Generate postrm script
cat << 'EOF' > "$BUILD_DIR/DEBIAN/postrm"
#!/bin/sh
set -e

if [ "$1" = "purge" ]; then
    echo "lnxCAD: purging service files..."
    rm -f /etc/systemd/system/lnxcad.service
    rm -f /etc/init.d/lnxcad
    rm -rf /etc/sv/lnxcad
    
    if which systemctl >/dev/null 2>&1; then
        systemctl daemon-reload || true
    fi
fi
EOF

# Set permissions of DEBIAN scripts
chmod 755 "$BUILD_DIR/DEBIAN/postinst"
chmod 755 "$BUILD_DIR/DEBIAN/prerm"
chmod 755 "$BUILD_DIR/DEBIAN/postrm"

echo "=== Building Debian Package ==="
if which dpkg-deb >/dev/null 2>&1; then
    if dpkg-deb --help | grep -q -- "--root-owner-group"; then
        dpkg-deb --root-owner-group --build "$BUILD_DIR"
    elif which fakeroot >/dev/null 2>&1; then
        fakeroot dpkg-deb --build "$BUILD_DIR"
    else
        dpkg-deb --build "$BUILD_DIR"
    fi
    echo "=== Package Built successfully: ${BUILD_DIR}.deb ==="
    # Cleanup directory
    rm -rf "$BUILD_DIR"
else
    echo "Error: dpkg-deb command not found. Cannot build .deb package." >&2
    echo "Ensure you are on a Debian-based system to run this script." >&2
    exit 1
fi
