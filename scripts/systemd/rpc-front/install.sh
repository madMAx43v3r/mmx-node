#!/bin/bash
# Called on each dedicated RPC host by deploy.py. Re-running preserves keys/certs.
set -euo pipefail
(( EUID == 0 )) || { echo 'Run as root' >&2; exit 1; }
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
state=/var/lib/mmx-rpc-front

case ${1:-} in
prepare)
    # Block package postinst from competing with the live Caddy listener.
    if ! command -v haproxy >/dev/null || ! command -v certbot >/dev/null; then
        systemctl mask haproxy.service
        trap 'systemctl unmask haproxy.service' EXIT
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq
        apt-get install -y -qq haproxy certbot python3 openssh-client ca-certificates curl
        systemctl unmask haproxy.service
        trap - EXIT
    fi
    command -v caddy >/dev/null
    caddy list-modules | grep -qx caddy.listeners.proxy_protocol
    install -d -m 0755 "$state" "$state/challenges" /etc/mmx-rpc-front
    install -m 0755 "$source_dir/frontend.py" /usr/local/sbin/mmx-rpc-front
    install -m 0644 "$source_dir"/*.service "$source_dir"/*.timer /etc/systemd/system/
    if [[ ! -f $state/id_ed25519 ]]; then
        ssh-keygen -q -t ed25519 -N '' -C mmx-rpc-acme -f "$state/id_ed25519"
    fi
    chmod 0600 "$state/id_ed25519"
    systemctl daemon-reload
    ;;
activate)
    test -f /etc/mmx-rpc-front/config.json
    domain=$(python3 -c 'import json; print(json.load(open("/etc/mmx-rpc-front/config.json"))["domain"])')
    backup=$(mktemp -d "$state/backup.XXXXXXXX")
    cp -a /etc/caddy/Caddyfile "$backup/Caddyfile"
    if [[ -f /etc/haproxy/haproxy.cfg ]]; then cp -a /etc/haproxy/haproxy.cfg "$backup/haproxy.cfg"; fi
    was_active=$(systemctl is-active haproxy || true)
    rollback() {
        trap - ERR
        cp -a "$backup/Caddyfile" /etc/caddy/Caddyfile
        if [[ -f $backup/haproxy.cfg ]]; then cp -a "$backup/haproxy.cfg" /etc/haproxy/haproxy.cfg; fi
        if [[ $was_active == active ]]; then systemctl reload haproxy || true; else systemctl stop haproxy || true; fi
        systemctl reload caddy || true
        echo "Activation failed; restored configuration from $backup" >&2
        exit 1
    }
    trap rollback ERR
    # Bootstrap from this host's existing certificate; no TLS key leaves the host.
    if [[ ! -L /etc/caddy/rpc-front/current ]]; then
        bootstrap=$(mktemp -d "$state/bootstrap.XXXXXXXX")
        if [[ -f /etc/caddy/rpc-certs/current/fullchain.pem ]]; then
            cp /etc/caddy/rpc-certs/current/{fullchain,privkey}.pem "$bootstrap/"
        else
            existing=/var/lib/caddy/.local/share/caddy/certificates/acme-v02.api.letsencrypt.org-directory/$domain
            if [[ -f $existing/$domain.crt ]]; then
                cp "$existing/$domain.crt" "$bootstrap/fullchain.pem"
                cp "$existing/$domain.key" "$bootstrap/privkey.pem"
            else
                # New nodes are not added to public DNS until independent issuance passes.
                install -d -m 0750 -o root -g caddy /etc/caddy/rpc-front /etc/caddy/rpc-front/bootstrap
                openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -days 2 \
                    -subj "/CN=$domain" -addext "subjectAltName=DNS:$domain" \
                    -keyout /etc/caddy/rpc-front/bootstrap/privkey.pem \
                    -out /etc/caddy/rpc-front/bootstrap/fullchain.pem 2>/dev/null
                chown root:caddy /etc/caddy/rpc-front/bootstrap/*.pem
                chmod 0640 /etc/caddy/rpc-front/bootstrap/*.pem
                ln -s bootstrap /etc/caddy/rpc-front/current
            fi
        fi
        if [[ -f $bootstrap/fullchain.pem ]]; then /usr/local/sbin/mmx-rpc-front bootstrap "$bootstrap"; fi
        rm -rf -- "$bootstrap"
    fi
    /usr/local/sbin/mmx-rpc-front prune offline
    /usr/local/sbin/mmx-rpc-front render > /etc/mmx-rpc-front/haproxy.next
    /usr/local/sbin/mmx-rpc-front render-caddy > /etc/mmx-rpc-front/Caddyfile.next
    haproxy -c -f /etc/mmx-rpc-front/haproxy.next
    caddy validate --adapter caddyfile --config /etc/mmx-rpc-front/Caddyfile.next
    systemctl enable --now mmx-rpc-acme.service
    install -m 0644 /etc/mmx-rpc-front/haproxy.next /etc/haproxy/haproxy.cfg
    install -m 0644 /etc/mmx-rpc-front/Caddyfile.next /etc/caddy/Caddyfile
    # A graceful Caddy reload releases public ports and starts the loopback sites.
    systemctl reload caddy
    systemctl enable haproxy
    if [[ $was_active == active ]]; then systemctl reload haproxy; else systemctl start haproxy; fi
    curl --fail --silent --max-time 10 -H "Host: $domain" http://127.0.0.1:8080/server/status >/dev/null
    tls_options=()
    if [[ $(readlink /etc/caddy/rpc-front/current) == bootstrap ]]; then
        tls_options=(--cacert /etc/caddy/rpc-front/bootstrap/fullchain.pem)
    fi
    verify_tls() {
        curl --fail --silent --show-error --max-time 10 "${tls_options[@]}" \
            --resolve "$domain:443:127.0.0.1" "https://$domain/server/status" >/dev/null
    }
    # Caddy 2.11 can evict the shared cache entry when managed -> file-loaded
    # activation removes the old server. A second forced reload restores it.
    if ! verify_tls; then
        caddy reload --config /etc/caddy/Caddyfile --force
        verify_tls
    fi
    systemctl enable --now mmx-rpc-acme-prune.timer
    # Retire copying only on this node. Do not contact any legacy gateway.
    systemctl disable --now mmx-rpc-cert-sync.timer 2>/dev/null || true
    systemctl stop mmx-rpc-cert-sync.service 2>/dev/null || true
    trap - ERR
    echo "Activated RPC frontend; configuration rollback: $backup"
    ;;
enable-renewal)
    domain=$(python3 -c 'import json; print(json.load(open("/etc/mmx-rpc-front/config.json"))["domain"])')
    test -f "/etc/letsencrypt/renewal/$domain.conf"
    systemctl disable --now certbot.timer
    systemctl enable --now mmx-rpc-renew.timer
    ;;
*) echo 'Usage: install.sh prepare|activate|enable-renewal' >&2; exit 2 ;;
esac
