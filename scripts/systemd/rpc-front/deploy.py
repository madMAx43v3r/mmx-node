#!/usr/bin/env python3
"""Repeatable RPC rollout, including forwarded ports; TLS keys stay on their host."""

import argparse
import io
import json
from pathlib import Path
import re
import shlex
import subprocess
import tarfile

import frontend
import build_caddy
import verify_http2

HERE = Path(__file__).resolve().parent


def ssh(node, command, payload=None, timeout=900):
    result = subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=yes",
                             "-o", "ConnectTimeout=10", "-p", str(node.get("ssh_port", 22)), node["ssh"], command],
                            input=payload, capture_output=True, timeout=timeout)
    if result.returncode:
        raise RuntimeError(f"{node['id']}: {command}\n" + result.stdout.decode()[-6000:] + result.stderr.decode()[-6000:])
    return result.stdout.decode()


def upload(node, caddy_binary):
    archive = io.BytesIO()
    with tarfile.open(fileobj=archive, mode="w:gz") as tar:
        for path in sorted(HERE.iterdir()):
            if path.suffix in (".py", ".sh", ".service", ".timer"):
                tar.add(path, arcname=path.name)
        tar.add(caddy_binary, arcname="caddy")
    # A fixed root-owned staging directory is never writable by service users.
    ssh(node, "install -d -m 0700 /var/lib/mmx-rpc-front-source && tar -xzf - -C /var/lib/mmx-rpc-front-source", archive.getvalue())


def configure(node, inventory, identities):
    config = dict(inventory, node_id=node["id"])
    frontend.validate_config(config)
    writer = "import os,sys,tempfile; fd,p=tempfile.mkstemp(dir='/etc/mmx-rpc-front'); f=os.fdopen(fd,'wb'); f.write(sys.stdin.buffer.read()); f.close(); os.chmod(p,0o644); os.replace(p,'/etc/mmx-rpc-front/config.json')"
    ssh(node, "python3 -c " + shlex.quote(writer), (json.dumps(config, indent=2) + "\n").encode())
    # Only routing claims/releases are authorized; the caller cannot choose its owner.
    entries, hosts = [], []
    for peer in inventory["nodes"]:
        if peer["id"] == node["id"]:
            continue
        public_key, host_key = identities[peer["id"]]
        for key in (public_key, host_key):
            if not re.fullmatch(r"ssh-ed25519 [A-Za-z0-9+/=]+(?: [^\r\n]+)?", key):
                raise ValueError("invalid public key")
        key = " ".join(public_key.split()[:2])
        entries.append(f'restrict,command="/usr/local/sbin/mmx-rpc-front peer {peer["id"]}" {key}')
        host = peer["address"] if peer.get("ssh_port", 22) == 22 else f'[{peer["address"]}]:{peer["ssh_port"]}'
        hosts.append(host + " " + " ".join(host_key.split()[:2]))
    payload = json.dumps({"keys": entries, "hosts": hosts}).encode()
    script = r'''
import json, os, pathlib, sys, tempfile
data = json.load(sys.stdin)
root = pathlib.Path('/root/.ssh')
root.mkdir(mode=0o700, exist_ok=True)
path = root / 'authorized_keys'
old = path.read_text() if path.exists() else ''
lines = [line for line in old.splitlines() if '/usr/local/sbin/mmx-rpc-front peer ' not in line
         and '/usr/local/sbin/mmx-install-rpc-certificate' not in line]
fd, pending = tempfile.mkstemp(dir=root)
with os.fdopen(fd, 'w') as output:
    output.write('\n'.join(lines + data['keys']) + '\n')
os.chmod(pending, 0o600)
os.replace(pending, path)
hosts = pathlib.Path('/var/lib/mmx-rpc-front/known_hosts')
hosts.write_text('\n'.join(data['hosts']) + '\n')
hosts.chmod(0o600)
'''
    ssh(node, "python3 -c " + shlex.quote(script), payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=Path, default=HERE / "nodes.json")
    parser.add_argument("--stage", choices=["prepare", "activate", "staging", "issue", "renewal", "all"], default="all")
    parser.add_argument("--caddy-binary", type=Path, help="Prebuilt pinned Caddy with HTTP/2 stream configuration")
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text())
    for node in inventory["nodes"]:
        frontend.validate_config(dict(inventory, node_id=node["id"]))
        if not re.fullmatch(r"root@[A-Za-z0-9.:-]+", node["ssh"]):
            raise ValueError("only explicit root SSH targets are supported")
    if args.stage in ("prepare", "all"):
        identities = {}
        binary = args.caddy_binary
        if binary is None:
            cache = Path.home() / ".cache/mmx-rpc-caddy"
            binary = cache / "caddy"
            build_caddy.build(binary, cache / "build")
        if "v2.11.7-mmx-http2" not in subprocess.check_output([str(binary.resolve()), "version"], text=True):
            raise ValueError("Caddy binary lacks the pinned HTTP/2 configuration patch")
        for node in inventory["nodes"]:
            print(node["id"] + ": preparing packages and local keys", flush=True)
            upload(node, binary)
            ssh(node, "bash /var/lib/mmx-rpc-front-source/install.sh prepare")
            keys = ssh(node, "cat /var/lib/mmx-rpc-front/id_ed25519.pub /etc/ssh/ssh_host_ed25519_key.pub").splitlines()
            if len(keys) != 2:
                raise RuntimeError("unexpected key metadata")
            identities[node["id"]] = keys
        for node in inventory["nodes"]:
            configure(node, inventory, identities)
            print(node["id"] + ": inventory and restricted peer access installed", flush=True)
    for stage in ("activate", "staging", "issue", "renewal"):
        if args.stage not in (stage, "all"):
            continue
        for node in inventory["nodes"]:
            print(node["id"] + ": " + stage, flush=True)
            if stage in ("activate", "renewal"):
                action = "enable-renewal" if stage == "renewal" else stage
                print(ssh(node, "bash /var/lib/mmx-rpc-front-source/install.sh " + action), flush=True)
            else:
                print(ssh(node, "/usr/local/sbin/mmx-rpc-front " + stage), flush=True)
    if args.stage in ("all", "renewal"):
        for node in inventory["nodes"]:
            domain = inventory["domain"]
            print(ssh(node, "curl --fail --silent --max-time 10 --resolve " + domain + ":443:127.0.0.1 https://" + domain + "/node/info"), flush=True)
            port = node.get("https_port", 443)
            verify_http2.verify(domain, node["address"], port,
                                inventory["limits"].get("http2_streams_per_connection", 32))
            address = "[" + node["address"] + "]" if ":" in node["address"] else node["address"]
            subprocess.run(["curl", "--fail", "--silent", "--show-error", "--noproxy", "*", "--max-time", "10",
                            "--resolve", f"{domain}:{port}:{address}", f"https://{domain}:{port}/node/info"], check=True)
            print(f"\n{node['id']}: public HTTPS and HTTP/2 stream limit verified", flush=True)


if __name__ == "__main__":
    main()
