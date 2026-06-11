#!/bin/sh
# install.sh - Robust installation and uninstallation script for lnxCAD
# Supports systemd, SysVinit (+inittab), runit, and rc.local fallback.

set -e

# Target paths
BIN_DEST="/usr/sbin/lnxcad"

# Show usage
usage() {
    echo "Usage: $0 [install|uninstall]"
    echo "  install   : Install lnxCAD watchdog and enable the startup service (default)"
    echo "  uninstall : Stop services and completely remove lnxCAD from the system"
    exit 1
}

# Ensure we are root
if [ "$(id -u)" -ne 0 ]; then
    echo "Error: This script must be run as root." >&2
    exit 1
fi

# Determine action
ACTION=${1:-install}
if [ "$ACTION" != "install" ] && [ "$ACTION" != "uninstall" ]; then
    usage
fi

# Detect init system
detect_init() {
    p1_comm=""
    if [ -f /proc/1/comm ]; then
        p1_comm=$(cat /proc/1/comm)
    fi

    # systemd check
    if [ "$p1_comm" = "systemd" ] || [ -d /run/systemd/system ]; then
        echo "systemd"
        return
    fi

    # runit check
    if [ "$p1_comm" = "runit" ] || [ "$p1_comm" = "runsvdir" ] || [ -d /run/runit ]; then
        echo "runit"
        return
    fi

    # OpenRC check
    if which rc-status >/dev/null 2>&1; then
        echo "openrc"
        return
    fi

    # SysVinit check
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

find_binary() {
    if [ -f "./lnxcad" ]; then
        echo "./lnxcad"
    elif [ -f "./precompiled_binairies/base/lnxcad" ]; then
        echo "./precompiled_binairies/base/lnxcad"
    else
        echo ""
    fi
}

do_install() {
    echo "=== Installing lnxCAD ==="
    
    # 1. Locate binary
    SRC_BIN=$(find_binary)
    if [ -z "$SRC_BIN" ]; then
        echo "Error: Could not find 'lnxcad' binary in current directory or 'precompiled_binairies/base/'." >&2
        echo "Please compile it first using ./build.sh or ensure precompiled binaries are present." >&2
        exit 1
    fi
    
    echo "Using source binary: $SRC_BIN"
    
    # 2. Copy binary
    mkdir -p "$(dirname "$BIN_DEST")"
    cp -f "$SRC_BIN" "$BIN_DEST"
    chmod 755 "$BIN_DEST"
    chown root:root "$BIN_DEST"
    echo "Installed binary to $BIN_DEST"
    
    # 3. Configure service depending on detected init
    echo "Detected init system: $INIT_SYSTEM"
    case "$INIT_SYSTEM" in
        systemd)
            echo "Configuring systemd service..."
            cat << 'EOF' > /etc/systemd/system/lnxcad.service
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
EOF
            chmod 644 /etc/systemd/system/lnxcad.service
            systemctl daemon-reload
            systemctl enable lnxcad.service
            systemctl start lnxcad.service
            ;;
            
        sysvinit_inittab)
            echo "Configuring SysVinit inittab respawn..."
            # Check if entry already exists
            if ! grep -q "^cad:2345:respawn:$BIN_DEST" /etc/inittab; then
                echo "" >> /etc/inittab
                echo "# lnxCAD Rescue Watchdog" >> /etc/inittab
                echo "cad:2345:respawn:$BIN_DEST" >> /etc/inittab
            fi
            # Reload init
            if which telinit >/dev/null 2>&1; then
                telinit q
            elif which init >/dev/null 2>&1; then
                init q
            fi
            # Launch manually if not running immediately
            if ! pgrep -f "$BIN_DEST" >/dev/null 2>&1; then
                "$BIN_DEST" &
            fi
            ;;
            
        sysvinit|openrc)
            echo "Configuring init.d script..."
            cat << 'EOF' > /etc/init.d/lnxcad
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

# Check if start-stop-daemon is available
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
    echo "Starting $NAME..."
    if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        echo "$NAME is already running"
        exit 0
    fi
    start_daemon
    ;;
  stop)
    echo "Stopping $NAME..."
    stop_daemon
    ;;
  restart)
    $0 stop
    sleep 1
    $0 start
    ;;
  status)
    if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
      echo "$NAME is running (PID $(cat "$PIDFILE"))"
      exit 0
    else
      echo "$NAME is not running"
      exit 3
    fi
    ;;
  *)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
    ;;
esac
EOF
            chmod 755 /etc/init.d/lnxcad
            
            # Register service
            if which rc-update >/dev/null 2>&1; then
                rc-update add lnxcad default
                rc-service lnxcad start
            elif which update-rc.d >/dev/null 2>&1; then
                update-rc.d lnxcad defaults
                update-rc.d lnxcad enable
                invoke-rc.d lnxcad start || /etc/init.d/lnxcad start
            elif which chkconfig >/dev/null 2>&1; then
                chkconfig --add lnxcad
                chkconfig lnxcad on
                service lnxcad start || /etc/init.d/lnxcad start
            else
                /etc/init.d/lnxcad start
            fi
            ;;
            
        runit)
            echo "Configuring runit service..."
            mkdir -p /etc/sv/lnxcad
            cat << 'EOF' > /etc/sv/lnxcad/run
#!/bin/sh
exec /usr/sbin/lnxcad
EOF
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
            else
                echo "Warning: runit service directory (/var/service or /etc/service) not found. Created service in /etc/sv/lnxcad."
            fi
            ;;
            
        fallback)
            echo "Configuring fallback via /etc/rc.local..."
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
                cat << EOF > /etc/rc.local
#!/bin/sh -e
$BIN_DEST &
exit 0
EOF
                chmod +x /etc/rc.local
            fi
            # Run it now
            if ! pgrep -f "$BIN_DEST" >/dev/null 2>&1; then
                "$BIN_DEST" &
            fi
            ;;
    esac

    # Verification
    sleep 0.5
    if pgrep -f "$BIN_DEST" >/dev/null 2>&1; then
        echo "=== Installation Summary ==="
        echo "Method used : $INIT_SYSTEM"
        echo "Status      : Active (lnxcad daemon is running)"
        echo "Binary      : Installed at $BIN_DEST"
    else
        echo "=== Installation Summary ==="
        echo "Method used : $INIT_SYSTEM"
        echo "Warning     : Installation complete, but the daemon does not seem to be running."
        echo "              Please check system logs or run: $BIN_DEST"
    fi
}

do_uninstall() {
    echo "=== Uninstalling lnxCAD ==="
    
    # Stop and clean up according to detected init
    echo "Stopping and removing service entries ($INIT_SYSTEM)..."
    case "$INIT_SYSTEM" in
        systemd)
            if systemctl is-active --quiet lnxcad.service 2>/dev/null; then
                systemctl stop lnxcad.service || true
            fi
            systemctl disable lnxcad.service 2>/dev/null || true
            rm -f /etc/systemd/system/lnxcad.service
            systemctl daemon-reload
            ;;
            
        sysvinit_inittab)
            if [ -f /etc/inittab ]; then
                sed -i "\@^cad:2345:respawn:$BIN_DEST@d" /etc/inittab
                # Clean up empty comments/lines if they exist
                sed -i '/# lnxCAD Rescue Watchdog/d' /etc/inittab
                if [ -s /etc/inittab ]; then
                    # Remove trailing empty lines
                    sed -i -e :a -e '/^\n*$/{$d;N;ba}' /etc/inittab
                fi
                if which telinit >/dev/null 2>&1; then
                    telinit q
                elif which init >/dev/null 2>&1; then
                    init q
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
            rm -f /etc/init.d/lnxcad
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
            which sv >/dev/null 2>&1 && sv stop lnxcad || true
            rm -rf /etc/sv/lnxcad
            ;;
            
        fallback)
            if [ -f /etc/rc.local ]; then
                sed -i "\@$BIN_DEST@d" /etc/rc.local
            fi
            pkill -f "$BIN_DEST" || true
            ;;
    esac

    # Ensure process is dead
    pkill -f "$BIN_DEST" || true
    
    # Remove binary
    if [ -f "$BIN_DEST" ]; then
        rm -f "$BIN_DEST"
        echo "Removed binary $BIN_DEST"
    fi
    
    echo "=== Uninstallation Summary ==="
    echo "Status      : Completely removed."
}

# Main Execution
case "$ACTION" in
    install)
        do_install
        ;;
    uninstall)
        do_uninstall
        ;;
esac
