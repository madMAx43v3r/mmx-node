# Public RPC frontend

The inventory in [nodes.json](nodes.json) manages eu1 (`80.47.12.16`), th1
(`64.176.84.253`) and eu2 (`80.47.12.14`). The legacy gateway is outside this deployment.
Eu2 uses forwarded public ports: SSH 1160 → 22, HTTP 1161 → 8080 and
HTTPS 1162 → 443. It runs the same frontend and limits inside the VM.

```text
HTTPS client -> HAProxy :443 (TCP admission, PROXY v2)
             -> Caddy 127.0.0.1:18443 (TLS, headers/body deadlines)
             -> HAProxy 127.0.0.1:9080 (HTTP request rates)
             -> existing RPC proxy 127.0.0.1:8081 -> MMX node

Plain HTTP   -> HAProxy :8080 -> Caddy 127.0.0.1:18080 -> same request limiter
HTTP :80     -> HTTPS redirect, except HTTP-01 and IP readiness probes
```

Caddy retains TLS termination, matching Ghostrack's public HAProxy / loopback
Caddy / PROXY v2 arrangement. Only loopback listeners accept PROXY headers.
Caddy overwrites the private client-address header from the actual PROXY v2
address; public `X-Forwarded-For` and `X-Mmx-Client` cannot change the counters.
HTTP/3 is not exposed: UDP cannot bypass the TCP admission limits.

## Limits

Each node independently enforces these defaults, shared across all three
public TCP ports:

| Scope | Concurrent client connections | Requests per 10 seconds |
| --- | ---: | ---: |
| Entire node | 4,096 | — |
| IPv4 address | 10 | 100 |
| IPv6 /64 | 10 | 100 |
| IPv6 /48, in addition to its /64 limits | 100 | 1,000 |

The public connection counters include sockets before TLS handshakes and count
actual clients, independently of Caddy's HTTP/2 streams or upstream pooling.

Each HTTP/2 connection additionally permits **32 concurrent streams**. This is
advertised and enforced at Caddy's TLS frontend, independently of connection
and request-rate budgets. A client violating the stream limit is refused at
the protocol layer before that stream reaches the backend. Change
`limits.http2_streams_per_connection` in the inventory to configure new nodes
and existing nodes together; older inventories default to 32.
An over-limit connection is closed immediately. HTTP request rates use
HAProxy's rolling `http_req_rate(10s)` counters; excess requests receive `429`
and `Retry-After: 10`. These are per-node budgets, not a shared cluster quota.
RPC requests, including cross-node fallback requests, pass through the same
request limiter. ACME challenge responses bypass the RPC request limiter.

Idle connections close after 15 seconds without traffic. Headers must arrive
within 5 seconds, and Caddy allows 5 seconds to read the request body, including
when bytes keep trickling in. Every plain HTTP response closes its connection;
this is verified by reading the socket to EOF, in addition to checking its
`Connection: close` header. Port 8080 retains the existing plain HTTP RPC and
cross-node fallback URLs. Port 80 redirects normal hostname requests to HTTPS;
IP readiness probes at `/` reach `/node/info`.

HAProxy's internal `maxconn` is twice the public client budget because each
active Caddy request can occupy a second loopback HAProxy connection. The
separate `node_total` table enforces the exact public budget. A graceful
HAProxy configuration reload starts fresh counters; existing connections drain
with the idle timeout. Certificate renewals reload Caddy only and preserve the
HAProxy counters. Neither proxy enables access/connection logging.

An upstream proxy/NAT that hides client addresses shares a source budget.
Do not trust public forwarded-address headers to work around this. A future
trusted upstream needs its own explicit PROXY/client-address arrangement.

## Independent certificates

Every node has its own ACME account, certificate lineage, private key, and
renewal timer. Certbot uses its documented automated manual HTTP-01 hooks:

1. The renewing node writes its token response to its own loopback responder.
2. Over pinned SSH connections, it asks each peer to route **that exact token**
   to its public HTTP port. The peer updates a HAProxy runtime map and its
   persistent map file; no proxy reload is required.
3. The renewer checks the response through every reachable peer before asking
   the CA to validate. Concurrent renewals use different tokens and coexist.
4. Certbot obtains the certificate directly from the CA. Its deploy hook
   validates the chain, domain, key match, and remaining lifetime, then switches
   a local certificate symlink and force-reloads Caddy. A TLS probe verifies the
   certificate actually served; activation failures restore the previous one.
5. Cleanup removes only this node's token routes. Abandoned routes expire after
   30 minutes; a one-minute timer reconciles the persistent and live maps.

Only public keys and routing requests move between nodes. TLS certificates,
ACME account keys, and TLS private keys stay on their issuing host. The old
certificate-copy timer and restricted receiver grant are retired on managed nodes.
Legacy machines are not modified and do not receive these new certificates.

The forced SSH command accepts only a bounded JSON `claim`/`release` message
with a validated token. The key fixes the caller's node ID; it cannot run shell
commands, use forwarding, change another owner's token, or select an arbitrary
destination. SSH host keys are read through the administrator's existing
verified SSH connections, then pinned independently for node-to-node access.

An unreachable public HTTP peer does not block healthy nodes from renewing.
A reachable peer that cannot route the token makes the hook fail, preserving
the existing certificate. DNS health checks must remove genuinely dead public
IPs; no HTTP-01 arrangement can make a CA connect to a dead IP. Lease cleanup
also handles a renewer disappearing midway through validation.

## Deployment and new nodes

Requirements: dedicated Debian/Ubuntu RPC hosts with the Caddy systemd service,
Python 3, the
existing RPC proxy on loopback port 8081, and root SSH access. The installer
adds the distribution HAProxy and Certbot packages. Establish and independently
verify the administrator's SSH host keys first: deployment refuses unknown keys.
The operator machine also needs Go with toolchain download support. Preparation
builds Caddy 2.11.7 with Go 1.26.8, verifies its pinned Go-module checksum and
adds the small server-scoped `http2_max_concurrent_streams` setting. Upstream
Caddy 2.11.7 does not expose that setting. Builds use two jobs and a locked
cache; no Go toolchain or compiler is installed on RPC hosts.

The custom executable lives under `/usr/local/lib/mmx-rpc-front`, selected by
a systemd drop-in, so distribution package updates cannot overwrite it. Caddy
continues running as its existing service user. Activation validates the new
config and restarts Caddy when its executable changes; rollback restores the
previous config and systemd override. The existing certificate/key stay local.

```sh
python3 scripts/systemd/rpc-front/deploy.py
```

To build/test once and reuse the binary explicitly:

```sh
nice -n 19 ionice -c 3 python3 scripts/systemd/rpc-front/build_caddy.py /tmp/mmx-rpc-caddy
python3 scripts/systemd/rpc-front/deploy.py --stage prepare --caddy-binary /tmp/mmx-rpc-caddy
```

This prepares **all** nodes and their restricted peer access before activating
any frontend, then tests staging issuance on each node, obtains each production
certificate, enables independent renewal, and verifies trusted HTTPS. It is
safe to repeat: local identities and production lineages are retained, and an
existing certificate is not forcibly reissued. All hosts in the inventory need
to be reachable during configuration rollout. If the command stops, correct
the reported problem and resume the appropriate stage:

```sh
python3 scripts/systemd/rpc-front/deploy.py --stage prepare
python3 scripts/systemd/rpc-front/deploy.py --stage activate
python3 scripts/systemd/rpc-front/deploy.py --stage staging
python3 scripts/systemd/rpc-front/deploy.py --stage issue
python3 scripts/systemd/rpc-front/deploy.py --stage renewal
```

To add a node, install its MMX node/RPC proxy and Caddy first, add its ID,
literal public IP, and root SSH destination to the inventory, then rerun the
deployment for the full inventory. Permit TCP 22/80/443/8080 as required by the
deployment network. Loopback ports 8888/9080/18080/18443 must stay private.
For NAT hosts, set `ssh_port`, `http_port` and `https_port` to the public mapped
ports (defaults 22, 80 and 443). Guest listeners remain unchanged. The HTTP
mapping must reach guest port 80 or 8080; peer challenge forwarding and probes
use that mapping, and peer SSH host keys are pinned with their mapped port.
The HTTPS mapping must reach guest port 443. HTTP-01 still requires the shared
domain's public port 80 to reach a participating peer; a high-port mapping alone
cannot satisfy the CA. Do not add a NAT address directly to shared DNS without
an appropriate port-80/443 gateway.
Add the new node to DNS only after staging, production issuance, trusted HTTPS,
and synchronization checks pass. A brand-new node without a local certificate
uses a temporary self-signed bootstrap certificate until production issuance;
this is never copied from a peer. Both existing nodes bootstrap from their own
existing certificate without exposing a temporary untrusted certificate.

Only update this dedicated Caddy instance with this installer. If it hosts
other sites, split them into a separate instance before using this deployment.
Configuration backups are retained under `/var/lib/mmx-rpc-front/backup.*`;
failed activation restores the previous proxy configurations. For rollback
after a successful activation, stop HAProxy, restore the backup Caddyfile and
HAProxy config, and reload Caddy. This restores the previous listeners but does
not reenable the retired copying system automatically.

Activation checks both HTTP transport and TLS. Caddy 2.11.7 can evict a cached
certificate when switching from managed to file-loaded certificates; the
installer retries a forced reload and verifies TLS before considering that
transition successful. Existing nodes keep a trusted certificate throughout
subsequent deployments.

Certbot's production exact-domain issuance limits still apply when adding many
nodes; stagger initial production issuances accordingly. Staging certificates
use a separate directory and have no deploy hook, so they cannot replace a
trusted production certificate. Renewals run four times daily with up to 30
minutes of jitter; a failed run is retried at the next scheduled check.

## Verification

```sh
python3 scripts/systemd/rpc-front/test_frontend.py
# Include high-port tests with the pinned Caddy build:
CADDY=/usr/local/lib/mmx-rpc-front/caddy python3 scripts/systemd/rpc-front/test_frontend.py
```

The RPC frontend workflow builds this same binary and runs all 18 regressions.
The HTTP/2 boundary fixture keeps 32 real backend requests active, checks the
advertised SETTINGS limit, deliberately opens a 33rd stream without client-side
queuing, verifies protocol rejection before backend admission, and checks that
ordinary requests still work. Other checks retain the body/idle deadlines,
IPv4/IPv6 aggregation, rate limits, source spoofing and ACME coordination.

On either node:

```sh
systemctl status haproxy caddy mmx-rpc-acme
systemctl list-timers mmx-rpc-renew.timer mmx-rpc-acme-prune.timer
systemctl start mmx-rpc-renew.service
journalctl -u mmx-rpc-renew.service --no-pager -n 30
curl --fail --resolve rpc.mmx.network:443:127.0.0.1 https://rpc.mmx.network/node/info
curl --include http://127.0.0.1:8080/node/info
python3 /var/lib/mmx-rpc-front-source/verify_http2.py rpc.mmx.network --address 127.0.0.1
```

Activation runs the same authenticated SETTINGS check and rolls back if the
live endpoint does not advertise the inventory's stream limit.

Sources: [Certbot validation hooks](https://eff-certbot.readthedocs.io/en/stable/using.html#pre-and-post-validation-hooks),
[HAProxy stick tables](https://www.haproxy.com/documentation/haproxy-configuration-tutorials/proxying-essentials/custom-rules/stick-tables/),
[Caddy timeouts and listener wrappers](https://caddyserver.com/docs/caddyfile/options).

## Deployment verification, 2026-11-11

Both eu1 and th1 passed independent staging and production issuance. Their
distinct production certificates expire on 2027-02-09; renewal and lease-prune
services completed successfully, both timers are enabled, and no challenge
leases remain. Caddy's internal HTTP/TLS listeners and the request limiter bind
only loopback; HAProxy owns public TCP 80/443/8080. The old copying timer is
disabled on th1.

The isolated suite passed 13 tests, including IPv6 /64 and /48 admission,
the shared total, request-rate grouping, client-header spoofing, plain HTTP
socket closure, HTTP/1 and HTTP/2 upload deadlines, concurrent challenge
ownership, expiry, unavailable peers, and recovery after runtime-map failure.
Live loopback probes on each node measured 15.00-second idle closure and
approximately 5.12-second slow-upload termination for both HTTP and HTTPS;
over-budget requests received 429. Repeated preparation and activation retained
the nodes' identities and independently issued certificates.
