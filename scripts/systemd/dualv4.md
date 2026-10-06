# dualv4 deployment

The node, RPC proxy, and Caddy are installed and enabled on dualv4. Caddy handles
certificate renewal, and the obsolete daily RPC restart timer has been removed.
Both MMX services run as `mad` from `/home/mad/mmx-node`. The node uses
`config/server/` and the RTX 4060 Ti by name for OpenCL. CUDA proof recomputation
is enabled, with visibility restricted to that GPU's UUID.
The RPC proxy listens on `127.0.0.1:8081`. Caddy has obtained a valid certificate
for `rpc.mmx.network`.

The gateway is `ubuntu@15.235.186.10`. HAProxy continues to serve production RPC
requests through t320. HTTP requests for `rpc.mmx.network` under
`/.well-known/acme-challenge/` go to dualv4 (`10.10.10.4:80`) so Caddy can obtain
and renew certificates while t320 serves production. Other HTTP requests and
HTTPS still use t320.

The short public HTTPS test against dualv4 returned `200` for `/server/status`,
verified its certificate, and returned the expected `503` for `/node/info` while
the node was unsynced. Production routing was restored and `/node/info` returned
`200` with `is_synced: true` from t320.

The existing node database began at height 208266. Its stale seed configuration
was updated from this repository's mainnet seed list, with the reachable gateway
added as a fixed peer. The node then connected to current peers and started
advancing. The source and submodules have since been updated to upstream commit
`41f2e4ed205dfad9e64081e3501547d6db54cb52`, and the node was rebuilt as
`v1.5.3-13-g41f2e4ed` with CUDA and OpenCL support. This includes hardfork 2 at
mainnet block 5,050,000. The existing database was retained.

The transaction/hash tests passed 17/17, VM engine tests 12/12, and VM storage
tests 10/10. With NVIDIA driver `610.57.04`, the CUDA recomputation reference
test passed three jobs at each compression level 0, 5, and 9, including exact
reconstruction of the expected proof inputs and matching proof outputs.

The build uses CUDA toolkit 13.4, while the driver reports CUDA 13.3 support.
PTX-only kernels failed the reference test with `zero matches at table 2` even
with the CUDA cache disabled. Adding native `sm_89` kernels for the 4060 Ti
resolved the failure without changing node source code. The deployed CMake
settings are:

```sh
cmake -S . -B build \
    -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc \
    -DCMAKE_CUDA_FLAGS="-gencode=arch=compute_89,code=sm_89" \
    -DCMAKE_CUDA_FLAGS_RELEASE="-O2 -DNDEBUG"
```

CUDA and OpenCL are both enabled on the 4060 Ti. After enabling CUDA, the node
advanced past height 608000 with no proof verification errors or service
restarts. Driver retest and build logs are in `tmp/cuda-driver-610-*.log` on dualv4.

Upgrade logs are in `tmp/hardfork-*.log` on dualv4. The old build, database, and
local configuration are backed up under
`/home/mad/mmx-node/tmp/before-hardfork-upgrade-20261004/`.

## Check synchronization

On dualv4:

```sh
cd /home/mad/mmx-node
source ./activate.sh
mmx node info -n localhost:11335
journalctl -u mmx-node -f
```

From the gateway, test dualv4 through Caddy without changing public routing:

```sh
curl --fail --resolve rpc.mmx.network:443:10.10.10.4 \
    https://rpc.mmx.network/node/info
```

Leave production on t320 until this returns `200`, reports `is_synced: true`, and
its height is current compared with `https://rpc.mmx.network/node/info`.

## Prepared gateway configuration

The gateway has these root-owned files:

- `/etc/haproxy/haproxy.cfg`: live configuration serving t320.
- `/etc/haproxy/haproxy.cfg.t320-staging`: the same routing, including Caddy's
  certificate validation path.
- `/etc/haproxy/haproxy.cfg.dualv4-ready`: the tested configuration routing
  `rpc.mmx.network` to dualv4. HTTP routes by Host and HTTPS passes TLS through
  by SNI, leaving other hostnames and the existing 11337/7777 forwarding intact.
- `/etc/haproxy/haproxy.cfg.before-dualv4-20261004`: the original configuration.

After verifying dualv4 is synced, validate and load the prepared configuration
on the gateway:

```sh
sudo haproxy -c -f /etc/haproxy/haproxy.cfg.dualv4-ready
sudo install -m 0644 /etc/haproxy/haproxy.cfg.dualv4-ready /etc/haproxy/haproxy.cfg
sudo systemctl reload haproxy
curl --fail https://rpc.mmx.network/node/info
```

To return to t320:

```sh
sudo install -m 0644 /etc/haproxy/haproxy.cfg.t320-staging /etc/haproxy/haproxy.cfg
sudo systemctl reload haproxy
```

Backups of the replaced launchers and local peer configuration are under
`/home/mad/mmx-node/tmp/systemd-backup-20261004T065503Z/` on dualv4.
