# Public RPC on 80.47.12.16

The fresh VM `80.47.12.16` (`mmx-node-eu-1`) was configured on 2026-10-13
with the same systemd node, RPC proxy, and Caddy setup as `64.176.84.253`.
All three services are enabled at boot. Both MMX services restart on exit;
Caddy restarts on failure. DNS and gateway routing were left unchanged.

At the user's request, this node syncs from genesis. No existing node was
paused and no blockchain snapshot was copied. Genesis sync began at
2026-10-13 09:13:21 UTC. Public WebAPI requests return `503` until sync and
the subsequent 18-block safety delay complete. Wait for `/node/info` to
return `200` and `is_synced: true` before adding the VM to public DNS.

## Node installation

The VM runs Ubuntu 26.04 LTS with eight vCPUs and approximately 21 GiB of RAM.
The installed MMX release is the official Ubuntu 24.04 Debian package
`mmx-node-1.5.3-amd64-ubuntu-24.04.deb`, build commit
`456268a729d09613c79a0b8d6a5fc606af9abce2`. Its SHA-256 was verified as
`9c4048dfccab33dccce8c1d21841a26d42ac5e7cfc4a9dd9ecf0050824f8320c`.
This release includes hardfork 2 at height 5,050,000.

Ubuntu 26.04 does not provide the package's required `libminiupnpc17` ABI.
The Ubuntu 24.04 package `libminiupnpc17` version `2.2.6-1build2` was downloaded
through apt on `64.176.84.253` and securely transferred to this VM. Its SHA-256
is `ca3e2c5f76ea87d7f055772784801dfaf7d7eeb3122416a90df341d211fab857`.
It requires only `libc6 (>= 2.17)` and installed successfully alongside the
VM's native libraries. The remaining MMX dependencies came from Ubuntu 26.04.
The node's shared-library dependencies all resolved and the node advanced
from genesis successfully.

The node runs as `mmx` from `/usr/share/mmx-node`, with data and unique local
authentication credentials under `/var/lib/mmx/`. Its startup wrapper is
`/usr/local/bin/mmx-node-start`, loading these config directories in order:

```text
config/mainnet/ config/node/ config/server/ /var/lib/mmx/config/local/
```

Local overrides use four node workers, two database workers, and eight API
workers. The server profile supplies 16 HTTP workers, 200 HTTP connections,
16 WebAPI backends, and 10,000 blocks of history. The node HTTP API binds to
`127.0.0.1:11380`. Wallet, farmer, harvester, and timelord services are disabled.
The installation script and log are `/root/install-mmx.sh` and
`/root/mmx-install.log`; the compatibility package is under `/root/mmx-compat-deps/`.

## RPC and HTTPS

The root-owned proxy files are under `/opt/mmx-rpc`, running as `mmx` with
Node.js `v24.21.0` at `/usr/local/bin/node`. Dependencies were installed with
`npm ci --omit=dev` from the repository lockfile. `remotes.json` is `[]`, so
the backend is this VM's own node. The proxy binds to `127.0.0.1:8081` and uses
the same service hardening drop-in as the other VM.

Caddy `2.11.7` was installed from its official stable apt repository.
`/etc/caddy/Caddyfile` serves `rpc.mmx.network`, proxies to `127.0.0.1:8081`,
and redirects HTTP to HTTPS. The existing managed certificate was securely
copied from dualv4 into Caddy's storage, allowing verified HTTPS before DNS
changes. It expires on 2027-01-02; automatic certificate management is enabled.
Future ACME validation must reach the Caddy instance renewing the certificate.

UFW allows SSH on TCP 22, node peers on TCP 11337, HTTP on TCP 80, and HTTPS
on TCP/UDP 443. Caddy's admin API and the internal HTTP services use loopback.
Deployment checks verified the TLS certificate, HTTP `308` redirect,
`/server/status` HTTP `200` with only the local backend, and the expected
`/node/info` HTTP `503` during genesis sync. All services were active and
enabled with zero automatic restarts, and no failed systemd units.

## Check readiness

```sh
curl --fail --resolve rpc.mmx.network:443:80.47.12.16 \
    https://rpc.mmx.network/node/info
ssh root@80.47.12.16 \
    'curl -sS http://127.0.0.1:11380/api/node/get_network_info'
ssh root@80.47.12.16 'journalctl -u mmx-node -n 20 --no-pager'
```

Before updating DNS, require `is_synced: true` and compare the height and a
recent block hash against an existing RPC node.
