#!/usr/bin/env python3
"""Verify an endpoint's authenticated HTTP/2 stream limit without an HTTP request."""

import argparse
import socket
import ssl


def verify(hostname, address, port=443, expected=32, cacert=None):
    context = ssl.create_default_context(cafile=cacert)
    context.set_alpn_protocols(["h2"])
    with socket.create_connection((address, port), timeout=10) as raw:
        with context.wrap_socket(raw, server_hostname=hostname) as connection:
            if connection.selected_alpn_protocol() != "h2":
                raise ValueError("endpoint did not negotiate HTTP/2")
            connection.sendall(b"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n" + bytes.fromhex("000000040000000000"))

            def read(count):
                result = b""
                while len(result) < count:
                    part = connection.recv(count - len(result))
                    if not part:
                        raise ValueError("endpoint closed before its HTTP/2 SETTINGS")
                    result += part
                return result

            for _ in range(10):
                header = read(9)
                length = int.from_bytes(header[:3], "big")
                if length > 16384:
                    raise ValueError("oversized initial HTTP/2 frame")
                payload = read(length)
                if header[3] == 4 and not header[4] & 1:
                    if length % 6 or int.from_bytes(header[5:], "big") != 0:
                        raise ValueError("invalid HTTP/2 SETTINGS frame")
                    settings = {int.from_bytes(payload[i:i+2], "big"): int.from_bytes(payload[i+2:i+6], "big")
                                for i in range(0, length, 6)}
                    if settings.get(3) != expected:
                        raise ValueError(f"expected {expected} concurrent streams, got {settings.get(3)}")
                    return
            raise ValueError("endpoint did not send its HTTP/2 SETTINGS")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("hostname")
    parser.add_argument("--address")
    parser.add_argument("--port", type=int, default=443)
    parser.add_argument("--expected", type=int, default=32)
    parser.add_argument("--cacert")
    args = parser.parse_args()
    verify(args.hostname, args.address or args.hostname, args.port, args.expected, args.cacert)
    print(f"{args.hostname} via {args.address or args.hostname}: HTTP/2 MAX_CONCURRENT_STREAMS={args.expected}")
