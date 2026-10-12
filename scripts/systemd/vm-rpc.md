# Second RPC VM

The VM at `64.176.84.253` was configured on 2026-10-12 to serve
`rpc.mmx.network` through Caddy. DNS and the existing gateway routing were left
unchanged. All three services (`mmx-node`, `mmx-rpc`, and `caddy`) are enabled
at boot. Both MMX services restart on exit; Caddy restarts on failure.

The installed mainnet node is the `mmx-node` Debian package version `1.5.3`,
build commit `456268a729d09613c79a0b8d6a5fc606af9abce2`, which includes hardfork 2
at height 5,050,000. It runs as `mmx` from `/usr/share/mmx-node`, with its existing
data under `/var/lib/mmx/`. `/usr/local/bin/mmx-node-start` loads these config
directories in order:

```text
config/mainnet/ config/node/ config/server/ /var/lib/mmx/config/local/
```

The local overrides retain four node workers and two database workers and use
eight API workers on the eight-vCPU VM. The server profile supplies 16 HTTP
workers, 200 HTTP connections, 16 WebAPI backends, and 10,000 blocks of history.
The node HTTP API remains bound to `127.0.0.1:11380`.

The RPC proxy files are root-owned under `/opt/mmx-rpc`. Its systemd service
runs as `mmx`, using Node.js `v24.21.0` at `/usr/local/bin/node`. Dependencies
were installed from the repository lockfile with `npm ci --omit=dev`.
`remotes.json` is `[]`, so requests use this VM's own node. The proxy listens
on `127.0.0.1:8081`, with a hardening drop-in at
`/etc/systemd/system/mmx-rpc.service.d/security.conf`.

Caddy `2.11.7` is installed from its official stable apt repository. Its
configuration at `/etc/caddy/Caddyfile` proxies `rpc.mmx.network` to
`127.0.0.1:8081` and redirects HTTP to HTTPS. The existing managed certificate
for that hostname was securely copied from dualv4 into Caddy's certificate
storage, allowing verified HTTPS before DNS changes. It expires on
2027-01-02; automatic certificate management remains enabled. Future ACME
validation must reach the Caddy instance renewing the certificate.

UFW allows TCP 80/443 and UDP 443, in addition to the existing SSH and node
peer ports. Caddy's admin API and both internal HTTP services remain on
loopback. Pre-deployment configuration backups are under
`/root/rpc-setup/before/` on the VM.

## Verify before changing DNS

```sh
curl --fail --resolve rpc.mmx.network:443:64.176.84.253 \
    https://rpc.mmx.network/node/info
curl --fail --resolve rpc.mmx.network:443:64.176.84.253 \
    https://rpc.mmx.network/server/status
ssh root@64.176.84.253 \
    'systemctl status mmx-node mmx-rpc caddy --no-pager'
```

Require `/node/info` to return `200` and `is_synced: true`, and compare its
height with the existing RPC. After a node restart, public WebAPI requests
temporarily return `503` while synchronization and its safety delay complete.
Update DNS separately when ready to send production traffic to this VM.

Deployment verification returned HTTP `200` with `is_synced: true` at height
5,467,918, matching the existing RPC. Both endpoints returned block hash
`C069E81BC7FE72C3B502A92E423734CFF8930A4C173DBAA4B71B256E3EEB3235`
at height 5,467,900. HTTPS certificate validation succeeded without bypassing
verification, HTTP redirected to HTTPS, and all three services were active
and enabled with zero automatic restarts.
