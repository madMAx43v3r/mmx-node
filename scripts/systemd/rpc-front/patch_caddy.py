#!/usr/bin/env python3
"""Add a server-scoped HTTP/2 stream setting to the pinned Caddy source."""

import json
from pathlib import Path
import sys


def overlay(source, output):
    source, output = Path(source), Path(output)
    changes = {
        "modules/caddyhttp/server.go": [(
            '\tMaxHeaderBytes int `json:"max_header_bytes,omitempty"`',
            '\tMaxHeaderBytes int `json:"max_header_bytes,omitempty"`\n\n'
            '\t// HTTP2MaxConcurrentStreams caps active streams on each HTTP/2 connection.\n'
            '\t// Zero preserves the upstream HTTP/2 default.\n'
            '\tHTTP2MaxConcurrentStreams uint32 `json:"http2_max_concurrent_streams,omitempty"`')],
        "modules/caddyhttp/app.go": [(
            "http2.ConfigureServer(srv.server, new(http2.Server))",
            "http2.ConfigureServer(srv.server, &http2.Server{MaxConcurrentStreams: srv.HTTP2MaxConcurrentStreams})")],
        "caddyconfig/httpcaddyfile/serveroptions.go": [(
            "\tMaxHeaderBytes            int\n",
            "\tMaxHeaderBytes            int\n\tHTTP2MaxConcurrentStreams uint32\n"), (
            '\t\tcase "max_header_size":',
            '\t\tcase "http2_max_concurrent_streams":\n'
            '\t\t\tvar value string\n'
            '\t\t\tif !d.AllArgs(&value) {\n\t\t\t\treturn nil, d.ArgErr()\n\t\t\t}\n'
            '\t\t\tlimit, err := strconv.ParseUint(value, 10, 32)\n'
            '\t\t\tif err != nil || limit == 0 {\n'
            '\t\t\t\treturn nil, d.Err("http2_max_concurrent_streams must be an integer from 1 to 4294967295")\n'
            '\t\t\t}\n\t\t\tserverOpts.HTTP2MaxConcurrentStreams = uint32(limit)\n\n'
            '\t\tcase "max_header_size":'), (
            "\t\tserver.MaxHeaderBytes = opts.MaxHeaderBytes\n",
            "\t\tserver.MaxHeaderBytes = opts.MaxHeaderBytes\n"
            "\t\tserver.HTTP2MaxConcurrentStreams = opts.HTTP2MaxConcurrentStreams\n")],
    }
    replacements = {}
    for relative, edits in changes.items():
        original = source / relative
        text = original.read_text()
        for before, after in edits:
            if text.count(before) != 1:
                raise ValueError(f"pinned Caddy source differs: {relative}")
            text = text.replace(before, after)
        patched = output / relative
        patched.parent.mkdir(parents=True, exist_ok=True)
        patched.write_text(text)
        replacements[str(original.resolve())] = str(patched.resolve())
    manifest = output / "overlay.json"
    manifest.write_text(json.dumps({"Replace": replacements}, indent=2) + "\n")
    return manifest


if __name__ == "__main__":
    print(overlay(*sys.argv[1:]))
