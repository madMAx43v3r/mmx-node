
## Install

Install Node.js and follow the [systemd setup](../../scripts/systemd/README.md)
to run the node and RPC proxy as services. The node service includes
`-c config/server/`. Set `remotes.json` to an array of remote RPC URLs, or `[]`
to use only the local node.

### HTTPS

For shared public RPC nodes, use the repeatable
[HAProxy/Caddy frontend deployment](../../scripts/systemd/rpc-front/README.md).
It adds public connection/request limits, keeps Caddy TLS termination on
loopback, and automates each node's independent certificate renewal.

Caddy terminates HTTPS for `rpc.mmx.network` and forwards requests to this proxy
on `127.0.0.1:8081`. The systemd setup includes a Caddyfile. The proxy's bind
address and port can be overridden with `RPC_HOST` and `RPC_PORT`.
