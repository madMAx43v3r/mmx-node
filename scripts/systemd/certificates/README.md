# Coordinated RPC certificates

Deployed on 2026-10-26 for `rpc.mmx.network` across these public addresses:

| Address | HTTPS server | Certificate role |
| --- | --- | --- |
| `15.235.186.10` | HAProxy forwards HTTPS to Caddy on dualv4 | Replica |
| `64.176.84.253` | Local Caddy | Issuer |
| `80.47.12.16` | Local Caddy | Replica |

Caddy on `64.176.84.253` is the sole issuer. It uses Let's Encrypt HTTP-01 with
TLS-ALPN validation disabled. Every public IP forwards HTTP requests for
`/.well-known/acme-challenge/*` to that issuer. The replicas forward only this
path directly to `64.176.84.253`, preserving Host `rpc.mmx.network`. HAProxy
also has a dedicated ACME path backend to that VM, before its normal hostname
HTTP routing to dualv4. This avoids resolving the shared RPC hostname again
while forwarding challenges.

The gateway's `fwd_80` frontend includes this rule before `dualv4_http` routing:

```haproxy
use_backend rpc_acme_issuer if mmx_rpc_host mmx_acme_path
```

The dedicated backend is:

```haproxy
backend rpc_acme_issuer
    mode http
    http-request set-header Host rpc.mmx.network
    server issuer 64.176.84.253:80 check
```

The setup initially used dualv4 as issuer. On 2026-10-26 renewal and distribution
were moved to `64.176.84.253`, retaining the existing production certificate and
ACME account. Dualv4's former distribution timer, service, configuration, and
deployment key were retired; its old key grants were revoked on the VMs.

The replicas load explicit certificate files, which disables their independent
ACME certificate management. HTTPS RPC requests continue to use each replica's
local MMX node. Other HTTP requests redirect to HTTPS. The gateway's IP-based
HTTP readiness check continues to query dualv4's `/node/info`.

## Distribution

`mmx-rpc-cert-sync.timer` runs on `64.176.84.253` at boot and every five minutes,
with up to 30 seconds of timer scheduling tolerance. Its oneshot service sends the
production certificate and private key to both replicas over SSH. It reads
the current Caddy files for every run, so successful automatic renewals are
distributed without a separate renewal hook.

Installed files on `64.176.84.253`:

- `/etc/caddy/Caddyfile`: [issuer configuration](Caddyfile.issuer).
- `/usr/local/sbin/mmx-sync-rpc-certificates`: [sender](sync-rpc-certificates.py).
- `/etc/mmx-rpc-cert-sync.json`: [deployment configuration](sync-config.json).
- `/var/lib/mmx-rpc-cert-sync/id_ed25519`: root-only deployment key.
- `/var/lib/mmx-rpc-cert-sync/known_hosts`: pinned, previously verified SSH host keys.
- `/etc/systemd/system/mmx-rpc-cert-sync.service` and `.timer`.

The targets are `80.47.12.16` directly and dualv4 at WireGuard address
`10.10.10.4` through `ubuntu@15.235.186.10`. The sender uses a nested SSH
`ProxyCommand` with pinned host keys for both the gateway and dualv4. The
gateway's authorized-key entry is restricted to this exact byte-stream relay:

```text
restrict,command="exec /usr/bin/nc 10.10.10.4 22" <new issuer's public SSH key>
```

It cannot run arbitrary gateway commands or enable SSH port forwarding. The
relayed connection then authenticates separately to dualv4 using its restricted
certificate receiver key. The gateway relays encrypted SSH data and does not
receive the TLS private key in plaintext. No SSH daemon changes or new listeners
are needed for this relay.

The private key and certificate source directory is
`/var/lib/caddy/.local/share/caddy/certificates/acme-v02.api.letsencrypt.org-directory/rpc.mmx.network/`.
The issuer is explicitly configured to use Let's Encrypt so this storage path
stays consistent with the sender configuration.

Installed files on each replica:

- `/etc/caddy/Caddyfile`: [replica configuration](Caddyfile.replica).
- `/usr/local/sbin/mmx-install-rpc-certificate`: [restricted receiver](install-rpc-certificate.py).
- `/etc/caddy/rpc-certs/current`: symlink to an installed certificate version.
- A `restrict,command="/usr/local/sbin/mmx-install-rpc-certificate"` entry in
  root's SSH authorized keys for the deployment key.

The key cannot run arbitrary SSH commands or open forwarding sessions. The
receiver accepts only a size-limited tar containing two regular files named
`fullchain.pem` and `privkey.pem`. It checks the certificate's trusted chain,
server purpose, hostname, matching private key, and at least seven days of
remaining validity before installation. No private-key contents are logged.

Each version is stored in a root-owned directory with group `caddy`; directories
are `0750` and certificate/key files are `0640`. The receiver atomically switches
the `current` symlink, validates the Caddy configuration, and gracefully reloads
Caddy. It checks the actual TLS certificate served on local port 443, including
trust and hostname verification. Failed activation restores the previous version
and attempts to reload it. Old versions remain available for recovery.

An unchanged certificate causes no reload when Caddy is already serving it.
If one replica is unreachable or rejects an update, the sender still attempts
the other replica, marks its systemd run failed, and retries at the next timer
interval. The previous certificates remain in use. No MMX node or RPC process
restart is part of renewal.

The issuer and its HTTP validation route must be available when renewal occurs.
Replicas continue serving their installed certificates if the issuer is down.
New RPC addresses must use the same challenge-forwarding configuration before
being added to DNS, and must be added to the sender's targets and pinned host keys.

## Operation and recovery

On `64.176.84.253`:

```sh
systemctl list-timers mmx-rpc-cert-sync.timer
systemctl start mmx-rpc-cert-sync.service
systemctl show mmx-rpc-cert-sync.service -p Result -p ExecMainStatus
journalctl -u mmx-rpc-cert-sync.service -n 20 --no-pager
```

An inactive oneshot service with `Result=success` and `ExecMainStatus=0` is
normal between timer runs. Configuration backups are under
`/root/rpc-certificate-setup/` on all three machines; the issuer migration's
backups are in its `pre-64-renewer/` subdirectory. The gateway HAProxy backup is
`/etc/haproxy/haproxy.cfg.before-renewer-64-20261026T073124Z`, with the previous
ready configuration saved under the same path plus `.dualv4-ready`.
Restoring the VMs' original `Caddyfile.before` also restores independent ACME
management, so use
the coordinated configuration for normal operation.

Check the installed certificate on a replica:

```sh
openssl x509 -in /etc/caddy/rpc-certs/current/fullchain.pem \
    -noout -dates -fingerprint -sha256
curl --fail --resolve rpc.mmx.network:443:127.0.0.1 \
    https://rpc.mmx.network/node/info
```

The receiver verifies live HTTPS after installation, so a replacement replica
must first be serving the current production certificate (as these VMs were).
Then run the receiver before activating its file-based Caddy configuration.
During the initial migration from a managed certificate to the same file-loaded certificate,
Caddy 2.11.7 needed a second forced reload before TLS checks passed. The
deployed replicas subsequently passed external HTTPS checks. Future certificate
updates are checked by the receiver and roll back on failure.

## Validation performed

- Local tests cover direct and relayed SSH command construction, archive
  traversal, links, duplicate/missing files, size limits, valid and untrusted
  certificate chains, wrong hostnames, mismatched
  keys, near-expiry certificates, and atomic activation rollback.
- Let's Encrypt staging successfully issued a certificate using HTTP-01 through
  the deployed forwarding path. An isolated Caddy instance on the new issuer's
  loopback ports 18080/18443 handled the test; it was stopped and the temporary challenge
  override removed afterward. Staging issuance on `64.176.84.253` succeeded at
  2026-10-26 07:32:10 UTC. The production certificate was retained.
- A separate routing probe verified the challenge path through every public IP
  and directly through dualv4's WireGuard HTTP listener.
- Live restricted SSH keys rejected arbitrary commands and unsafe archive paths
  on both replicas, including through the gateway relay. The systemd sender
  successfully distributed certificates and verified subsequent unchanged runs.
- A harmless alternate PEM serialization of the production certificate exercised
  a new version install and graceful reload on each replica. The systemd sender
  then restored the issuer's exact original files, with live TLS verified after
  both updates. The timer's scheduled run also completed successfully.
- All three public endpoints passed certificate validation, served the same
  production certificate, and returned HTTP `200` with `is_synced: true` after
  the migration. The gateway HTTP DNS readiness check returned `200` as well.

Run the local tests with:

```sh
python3 scripts/systemd/certificates/test_certificates.py
```

See Caddy's [TLS directive](https://caddyserver.com/docs/caddyfile/directives/tls)
for manual certificates and ACME challenge options, and
[automatic HTTPS](https://caddyserver.com/docs/automatic-https) for certificate
automation behavior.
