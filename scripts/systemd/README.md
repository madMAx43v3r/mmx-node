# MMX RPC services

These system services run the node with `-c config/server/` and the RPC proxy
without PM2. They restart on exit and write output to the journal. Caddy manages
certificate renewal without scheduled RPC restarts. The installer removes the
obsolete RPC restart timer from earlier installations.

Install a supported Node.js runtime and
[Caddy](https://caddyserver.com/docs/install#debian-ubuntu-raspbian), then install
the RPC dependencies:

```sh
cd /path/to/mmx-node/www/rpc-server
npm ci --omit=dev
# For a proxy using only its local node; otherwise supply your remote URLs.
test -f remotes.json || printf '[]\n' > remotes.json
cd ../..
sudo bash scripts/systemd/install.sh "$PWD" "$USER" "$(command -v node)"
```

The node runs as the specified account and retains the existing network, data,
and local configuration through `activate.sh`. The proxy runs as the same account
and listens on `127.0.0.1:8081`. Caddy owns ports 80/443 and manages HTTPS
certificates and renewal for `rpc.mmx.network`. Set the domain's DNS to the public
entry point for this host and route TCP ports 80/443 to Caddy.

For a dedicated Caddy instance, install the provided configuration:

```sh
sudo install -m 0644 scripts/systemd/Caddyfile /etc/caddy/Caddyfile
sudo caddy validate --config /etc/caddy/Caddyfile
```

If Caddy already serves other sites, add this site block to its existing
configuration instead.

If PM2 manages these processes, delete only the corresponding node and RPC
entries and run `pm2 save` under their owning account before starting systemd.
Remove any screen, cron, or other launch entries for these processes as well.

```sh
sudo systemctl enable --now mmx-node.service mmx-rpc.service
sudo systemctl enable --now caddy.service
sudo systemctl reload caddy.service
systemctl status mmx-node.service mmx-rpc.service
journalctl -u mmx-node.service -u mmx-rpc.service -f
curl --fail http://127.0.0.1:8081/server/status
curl --fail http://127.0.0.1:8081/node/info
curl --fail https://rpc.mmx.network/node/info
```

Use `sudo systemctl restart mmx-node` or `sudo systemctl restart mmx-rpc` after
changing their configuration. The RPC startup helper starts `mmx-rpc.service`.

For a host with multiple GPUs, add a node service override. This example selects
the first RTX 4060 Ti by name for OpenCL and its UUID for CUDA:

```ini
# /etc/systemd/system/mmx-node.service.d/gpu.conf
[Service]
Environment=CUDA_VISIBLE_DEVICES=GPU-your-4060-ti-uuid
ExecStart=
ExecStart=/path/to/mmx-node/run_node.sh -c config/server/ --Node.opencl_device_name "NVIDIA GeForce RTX 4060 Ti" --Node.opencl_device 0 --cuda.devices [0]
```

Obtain the CUDA UUID using `nvidia-smi --query-gpu=name,uuid --format=csv,noheader`.
Then run `sudo systemctl daemon-reload` and restart the node. Check the journal
for the selected OpenCL and CUDA device names. CUDA indices are relative to the
devices made visible by `CUDA_VISIBLE_DEVICES`.

`dualv4-gpu.conf` contains the verified GPU UUID and node path for dualv4. Install
it as `/etc/systemd/system/mmx-node.service.d/gpu.conf` on that host. It enables
CUDA proof recomputation and OpenCL VDF verification, both restricted to the
4060 Ti. That host's build includes native CUDA kernels for this GPU; see the
deployment notes for the CMake flag.

See [dualv4 deployment](dualv4.md) for the deployed routing, verification results,
and the prepared gateway configuration to load once the node is synced.
