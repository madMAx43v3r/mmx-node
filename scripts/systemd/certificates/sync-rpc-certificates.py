#!/usr/bin/python3
"""Send the issuer's current certificate to restricted receiver keys."""

import fcntl
import io
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tarfile


def ssh_command(config, target):
    address = target if isinstance(target, str) else target["host"]
    base = ["/usr/bin/ssh", "-F", "/dev/null", "-T",
            "-i", config["identity_file"],
            "-o", "BatchMode=yes", "-o", "IdentitiesOnly=yes",
            "-o", "StrictHostKeyChecking=yes",
            "-o", "UserKnownHostsFile=" + config["known_hosts_file"],
            "-o", "ConnectTimeout=10", "-o", "ServerAliveInterval=10",
            "-o", "ServerAliveCountMax=2"]
    command = list(base)
    if isinstance(target, dict) and target.get("relay"):
        # The relay's restricted key runs only nc to dualv4:22. No SSH
        # forwarding or unrestricted remote command is enabled for this key.
        relay = base + [target["relay"], "mmx-certificate-relay"]
        command += ["-o", "ProxyCommand=" + shlex.join(relay)]
    command.append("root@" + address)
    return address, command


def main():
    os.umask(0o077)
    config = json.loads(Path("/etc/mmx-rpc-cert-sync.json").read_text())
    with open("/run/lock/mmx-rpc-cert-sync.lock", "w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        source = Path(config["certificate_directory"])
        files = {
            "fullchain.pem": (source / "rpc.mmx.network.crt").read_bytes(),
            "privkey.pem": (source / "rpc.mmx.network.key").read_bytes(),
        }
        payload = io.BytesIO()
        with tarfile.open(fileobj=payload, mode="w:") as archive:
            for name, content in files.items():
                entry = tarfile.TarInfo(name)
                entry.size = len(content)
                entry.mode = 0o600
                archive.addfile(entry, io.BytesIO(content))
        failed = False
        for target in config["targets"]:
            address, command = ssh_command(config, target)
            try:
                result = subprocess.run(command, input=payload.getvalue(),
                                        capture_output=True, timeout=60)
                print(address + ": " + result.stdout.decode().strip(), flush=True)
                if result.returncode:
                    print(address + ": " + result.stderr.decode().strip(), file=sys.stderr)
                    failed = True
            except (OSError, subprocess.TimeoutExpired) as error:
                print(f"{address}: {error}", file=sys.stderr)
                failed = True
        return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
