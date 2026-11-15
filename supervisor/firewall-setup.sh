#!/bin/sh
# Confine the meshtasticd TCP API to loopback.
#
# meshtasticd (portduino) binds its API on ALL interfaces and offers no
# bind-address option, so on a networked device anyone could reach the radio
# API directly. The supervisor pins the API to one fixed port (API_PORT in
# supervisor.py); this rule blocks that port from every interface except
# loopback. Run once as root — the Debian package's postinst installs it.
#
# Usage:  sudo ./firewall-setup.sh [remove]
set -eu

PORT="${CZ_MESH_API_PORT:-47400}"

add_rules() {
    if command -v nft >/dev/null 2>&1; then
        nft add table inet cz_mesh 2>/dev/null || true
        nft 'add chain inet cz_mesh input { type filter hook input priority -1 ; policy accept ; }' 2>/dev/null || true
        # idempotent: only add if not already present
        if ! nft list chain inet cz_mesh input 2>/dev/null | grep -q "tcp dport $PORT"; then
            nft add rule inet cz_mesh input iif != "lo" tcp dport "$PORT" drop
        fi
        echo "nftables: confined meshtasticd API port $PORT to loopback"
    elif command -v iptables >/dev/null 2>&1; then
        iptables -C INPUT -p tcp --dport "$PORT" '!' -i lo -j DROP 2>/dev/null || \
            iptables -I INPUT -p tcp --dport "$PORT" '!' -i lo -j DROP
        echo "iptables: confined meshtasticd API port $PORT to loopback"
    else
        echo "WARNING: neither nft nor iptables found; API port $PORT is NOT confined" >&2
        exit 1
    fi
}

remove_rules() {
    if command -v nft >/dev/null 2>&1; then
        nft delete table inet cz_mesh 2>/dev/null || true
    fi
    if command -v iptables >/dev/null 2>&1; then
        iptables -D INPUT -p tcp --dport "$PORT" '!' -i lo -j DROP 2>/dev/null || true
    fi
    echo "removed meshtasticd API confinement for port $PORT"
}

case "${1:-add}" in
    remove) remove_rules ;;
    *) add_rules ;;
esac
