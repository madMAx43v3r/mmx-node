#!/usr/bin/env python3
"""Build pinned Caddy with configurable HTTP/2 stream admission, on the operator host."""

import argparse
import fcntl
import json
import os
from pathlib import Path
import shutil
import subprocess

from patch_caddy import overlay


CADDY = "v2.11.7"
CADDY_SUM = "h1:yj0Y4fYZGPkSvibBJ1sTWE33xC0fxztVyXEW5iIdUT4="
GO = "go1.26.8"


def build(output, cache):
    output, cache = Path(output).resolve(), Path(cache).resolve()
    cache.mkdir(parents=True, exist_ok=True)
    with (cache / "build.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        _build(output, cache)


def _build(output, cache):
    output.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, GOTOOLCHAIN=GO, GOMAXPROCS="2", CGO_ENABLED="0", GOOS="linux", GOARCH="amd64",
               GOCACHE=str(cache / "go-cache"), GOMODCACHE=str(cache / "go-modules"))

    def go(*args, capture=False):
        return subprocess.run(["go", *args], cwd=cache, env=env, check=True,
                              stdout=subprocess.PIPE if capture else None, text=True).stdout

    (cache / "go.mod").write_text(f"module mmx.network/rpc-caddy\n\ngo 1.26.0\n\nrequire github.com/caddyserver/caddy/v2 {CADDY}\n")
    module = json.loads(go("mod", "download", "-json", f"github.com/caddyserver/caddy/v2@{CADDY}", capture=True))
    if module.get("Sum") != CADDY_SUM:
        raise ValueError("Caddy module does not match the pinned source checksum")
    # Go intentionally forbids overlays beneath its immutable module cache.
    # Keep an explicit local source replacement and the original pinned hash.
    source = cache / f"caddy-source-{CADDY}"
    if not source.exists():
        shutil.copytree(module["Dir"], source)
    with (cache / "go.mod").open("a") as output_mod:
        output_mod.write(f"\nreplace github.com/caddyserver/caddy/v2 => ./{source.name}\n")
    manifest = overlay(source, cache / "patched")
    (cache / "main.go").write_text('package main\n\nimport (\n'
                                  ' caddycmd "github.com/caddyserver/caddy/v2/cmd"\n'
                                  ' _ "github.com/caddyserver/caddy/v2/modules/standard"\n'
                                  ')\n\nfunc main() { caddycmd.Main() }\n')
    go("build", "-p", "2", "-mod=mod", "-trimpath", "-buildvcs=false", "-overlay", str(manifest),
       "-ldflags", f"-s -w -X github.com/caddyserver/caddy/v2.CustomVersion={CADDY}-mmx-http2",
       "-o", str(output) + ".pending", ".")
    os.replace(str(output) + ".pending", output)
    subprocess.run([str(output), "version"], check=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--cache", type=Path, default=Path.home() / ".cache/mmx-rpc-caddy")
    args = parser.parse_args()
    build(args.output, args.cache)
