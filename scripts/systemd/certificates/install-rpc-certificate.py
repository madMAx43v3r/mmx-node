#!/usr/bin/python3
"""Restricted SSH receiver: validate, atomically install, and reload one TLS pair."""

import fcntl
import grp
import hashlib
import io
import os
from pathlib import Path
import shutil
import socket
import ssl
import subprocess
import sys
import tarfile
import tempfile
import time

DOMAIN = "rpc.mmx.network"
ROOT = Path("/etc/caddy/rpc-certs")
CAFILE = "/etc/ssl/certs/ca-certificates.crt"
FILES = {"fullchain.pem", "privkey.pem"}
MAX_BUNDLE = 131072


def run(*args):
    return subprocess.run(args, check=True, capture_output=True).stdout


def read_bundle(stream):
    data = stream.read(MAX_BUNDLE + 1)
    if len(data) > MAX_BUNDLE:
        raise ValueError("certificate bundle is too large")
    result = {}
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:") as archive:
        for entry in archive:
            if entry.name not in FILES or entry.name in result or not entry.isfile():
                raise ValueError("bundle must contain only fullchain.pem and privkey.pem")
            if entry.size > 65536:
                raise ValueError("certificate file is too large")
            result[entry.name] = archive.extractfile(entry).read()
    if result.keys() != FILES:
        raise ValueError("incomplete certificate bundle")
    return result


def validate(directory, cafile=CAFILE):
    cert, key = directory / "fullchain.pem", directory / "privkey.pem"
    leaf = directory / "leaf.pem"
    run("openssl", "x509", "-in", str(cert), "-out", str(leaf))
    run("openssl", "verify", "-CAfile", str(cafile), "-untrusted", str(cert),
        "-purpose", "sslserver", "-verify_hostname", DOMAIN, str(leaf))
    run("openssl", "x509", "-in", str(cert), "-checkend", "604800", "-noout")
    public_cert = run("openssl", "x509", "-in", str(cert), "-pubkey", "-noout")
    public_key = run("openssl", "pkey", "-in", str(key), "-pubout")
    if public_cert != public_key:
        raise ValueError("certificate and private key do not match")
    return run("openssl", "x509", "-in", str(cert), "-outform", "DER")


def check_served(expected):
    context = ssl.create_default_context()
    for attempt in range(3):
        try:
            with socket.create_connection(("127.0.0.1", 443), timeout=5) as raw:
                with context.wrap_socket(raw, server_hostname=DOMAIN) as tls:
                    if tls.getpeercert(binary_form=True) == expected:
                        return
        except (OSError, ssl.SSLError):
            pass
        if attempt < 2:
            time.sleep(1)
    raise ValueError("Caddy is not serving the installed certificate")


def reload_caddy():
    run("/usr/bin/caddy", "validate", "--config", "/etc/caddy/Caddyfile")
    run("/usr/bin/systemctl", "reload", "caddy")


def activate(root, version, reload=reload_caddy, check=lambda: None):
    current = root / "current"
    previous = os.readlink(current) if current.is_symlink() else None
    if previous == version:
        try:
            check()
        except ValueError:
            reload()
            check()
        return "unchanged"
    pending = root / ".current-next"
    pending.unlink(missing_ok=True)
    pending.symlink_to(version)
    os.replace(pending, current)
    try:
        reload()
        check()
    except Exception:
        if previous is not None:
            pending.symlink_to(previous)
            os.replace(pending, current)
        else:
            current.unlink()
        try:
            reload()
        except Exception:
            pass
        raise
    return "updated"


def main():
    if os.geteuid() != 0 or os.environ.get("SSH_ORIGINAL_COMMAND"):
        raise ValueError("only certificate installation is allowed")
    os.umask(0o027)
    with open("/run/lock/mmx-rpc-cert-install.lock", "w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        bundle = read_bundle(sys.stdin.buffer)
        gid = grp.getgrnam("caddy").gr_gid
        ROOT.mkdir(mode=0o750, parents=True, exist_ok=True)
        os.chown(ROOT, 0, gid)
        with tempfile.TemporaryDirectory(prefix=".incoming-", dir=ROOT) as staging:
            staging = Path(staging)
            for name, content in bundle.items():
                (staging / name).write_bytes(content)
            expected = validate(staging)
            (staging / "leaf.pem").unlink()
            version = hashlib.sha256(bundle["fullchain.pem"] + bundle["privkey.pem"]).hexdigest()
            destination = ROOT / version
            if not destination.exists():
                shutil.copytree(staging, destination)
                os.chmod(destination, 0o750)
                os.chown(destination, 0, gid)
                for name in FILES:
                    os.chmod(destination / name, 0o640)
                    os.chown(destination / name, 0, gid)
            result = activate(ROOT, version, check=lambda: check_served(expected))
            print(f"{DOMAIN}: {result}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        # Do not print archive contents, key material, or subprocess output.
        print(f"certificate installation failed: {error}", file=sys.stderr)
        sys.exit(1)
