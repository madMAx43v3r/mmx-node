#!/bin/bash
set -euo pipefail

if (( EUID != 0 )); then
    echo "Run as root: sudo $0 /path/to/mmx-node node-user [/path/to/node]" >&2
    exit 1
fi

source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
mmx_dir=$(realpath -- "${1:-$source_dir/../..}")
mmx_user=${2:-${SUDO_USER:-root}}
node_bin=${3:-$(command -v node || true)}

# Keep template substitutions and systemd command paths unambiguous.
if [[ ! $mmx_dir =~ ^/[a-zA-Z0-9_./-]+$ || ! $mmx_user =~ ^[a-zA-Z0-9_-]+$ || ! $node_bin =~ ^/[a-zA-Z0-9_./-]+$ ]]; then
    echo "Use absolute paths without spaces and a valid account name; Node.js must be installed." >&2
    exit 1
fi
id "$mmx_user" >/dev/null
test -x "$mmx_dir/run_node.sh"
test -x "$node_bin"
test -f "$mmx_dir/www/rpc-server/index.js"
test -d "$mmx_dir/www/rpc-server/node_modules"

unit_dir=$(mktemp -d)
trap 'rm -rf -- "$unit_dir"' EXIT
for unit in mmx-node.service mmx-rpc.service; do
    sed -e "s|@MMX_DIR@|$mmx_dir|g" \
        -e "s|@MMX_USER@|$mmx_user|g" \
        -e "s|@NODE_BIN@|$node_bin|g" \
        "$source_dir/$unit" > "$unit_dir/$unit"
done
systemd-analyze verify "$unit_dir"/*
install -m 0644 "$unit_dir"/* /etc/systemd/system/

# Caddy handles certificate renewal; retire the old certificate-reload schedule.
if [[ -f /etc/systemd/system/mmx-rpc-restart.timer ]]; then
    systemctl disable --now mmx-rpc-restart.timer
fi
if [[ -f /etc/systemd/system/mmx-rpc-restart.service ]]; then
    systemctl stop mmx-rpc-restart.service
fi
rm -f /etc/systemd/system/mmx-rpc-restart.timer /etc/systemd/system/mmx-rpc-restart.service

systemctl daemon-reload
echo "Installed. Configure remotes.json, stop the old processes, then run:"
echo "  sudo systemctl enable --now mmx-node.service mmx-rpc.service"
