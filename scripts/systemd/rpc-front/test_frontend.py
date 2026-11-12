#!/usr/bin/env python3
"""Unit tests plus real HAProxy/Caddy tests on isolated high ports."""

import http.client
import http.server
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import tempfile
import threading
import time
import unittest
from unittest import mock

import frontend
import deploy
import verify_http2

HERE = Path(__file__).resolve().parent


def config():
    return dict(json.loads((HERE / "nodes.json").read_text()), node_id="eu1")


class ClaimsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.state = Path(self.temp.name)
        self.patches = [mock.patch.object(frontend, "STATE", self.state),
                        mock.patch.object(frontend, "MAP", self.state / "acme.map")]
        for patch in self.patches:
            patch.start()
        self.config = config()

    def tearDown(self):
        for patch in self.patches:
            patch.stop()
        self.temp.cleanup()

    def test_simultaneous_owners_route_only_their_own_tokens(self):
        frontend.change_claim(self.config, "eu1", "claim", "a" * 32, online=False)
        frontend.change_claim(self.config, "th1", "claim", "b" * 32, online=False)
        routes = frontend.MAP.read_text()
        self.assertIn("a" * 32 + " acme_local", routes)
        self.assertIn("b" * 32 + " acme_th1", routes)
        with self.assertRaisesRegex(ValueError, "different node"):
            frontend.change_claim(self.config, "th1", "release", "a" * 32, online=False)
        frontend.change_claim(self.config, "eu1", "release", "a" * 32, online=False)
        self.assertNotIn("a" * 32, frontend.MAP.read_text())
        self.assertIn("b" * 32, frontend.MAP.read_text())

    def test_stale_claims_expire_and_survive_restarts_until_then(self):
        with mock.patch.object(frontend.time, "time", return_value=100):
            frontend.change_claim(self.config, "th1", "claim", "b" * 32, online=False)
        self.assertIn("acme_th1", frontend.MAP.read_text())
        with mock.patch.object(frontend.time, "time", return_value=100 + frontend.LEASE_SECONDS + 1):
            frontend.change_claim(self.config, "eu1", "prune", online=False)
        self.assertEqual(frontend.MAP.read_text(), "")

    def test_untrusted_peer_and_unsafe_tokens_are_rejected(self):
        for owner, token in [("outsider", "a" * 32), ("th1", "../../etc/passwd"),
                             ("th1", "a" * 32 + "\nother backend"), ("th1", "x")]:
            with self.assertRaises(ValueError):
                frontend.change_claim(self.config, owner, "claim", token, online=False)

    def test_healthy_peer_with_broken_coordination_aborts_and_cleans_up(self):
        with mock.patch.object(frontend, "change_claim"), mock.patch.object(frontend, "tell_peer", side_effect=OSError("offline")), \
                mock.patch.object(frontend, "challenge_get", return_value="wrong"):
            with self.assertRaisesRegex(RuntimeError, "wrong challenge"):
                frontend.auth(self.config, "a" * 32, "v" * 48)
        self.assertFalse((self.state / "challenges" / ("a" * 32)).exists())

    def test_unreachable_peer_does_not_block_healthy_issuance(self):
        def get(address, domain, token, port=80):
            if address != "127.0.0.1":
                raise OSError("offline")
            return "v" * 48
        with mock.patch.object(frontend, "change_claim"), mock.patch.object(frontend, "tell_peer", side_effect=OSError("offline")), \
                mock.patch.object(frontend, "challenge_get", side_effect=get):
            frontend.auth(self.config, "a" * 32, "v" * 48)
        self.assertTrue((self.state / "challenges" / ("a" * 32)).exists())

    def test_forwarded_peer_ports_and_authentication(self):
        peer = next(node for node in self.config["nodes"] if node["id"] == "eu2")
        command = frontend.peer_command(peer)
        self.assertEqual(command[command.index("-p") + 1], "1160")
        self.assertIn("StrictHostKeyChecking=yes", command)
        rendered = frontend.render_haproxy(self.config)
        self.assertIn("server owner 80.47.12.14:1161", rendered)
        with mock.patch.object(frontend, "change_claim"), mock.patch.object(frontend, "tell_peer"), \
                mock.patch.object(frontend, "challenge_get", return_value="v" * 48) as get:
            frontend.auth(self.config, "a" * 32, "v" * 48)
        get.assert_any_call(peer["address"], self.config["domain"], "a" * 32, 1161)

    def test_invalid_forwarded_ports_are_rejected(self):
        for key in ("ssh_port", "http_port", "https_port"):
            for port in (True, 0, -1, 65536, "1160"):
                value = config()
                value["nodes"][-1][key] = port
                with self.assertRaisesRegex(ValueError, "invalid node port"):
                    frontend.validate_config(value)

    def test_deployment_pins_forwarded_ssh_host_key(self):
        peer = next(node for node in self.config["nodes"] if node["id"] == "eu2")
        with mock.patch.object(deploy.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, b"", b"")) as run:
            deploy.ssh(peer, "true")
        command = run.call_args.args[0]
        self.assertEqual(command[command.index("-p") + 1], "1160")
        identities = {node["id"]: ("ssh-ed25519 AAAA", "ssh-ed25519 BBBB") for node in self.config["nodes"]}
        with mock.patch.object(deploy, "ssh") as ssh:
            deploy.configure(self.config["nodes"][0], self.config, identities)
        payload = json.loads(ssh.call_args.args[2])
        self.assertIn("[80.47.12.14]:1160 ssh-ed25519 BBBB", payload["hosts"])
        self.assertIn("64.176.84.253 ssh-ed25519 BBBB", payload["hosts"])

    def test_config_rejects_duplicate_nodes_and_boolean_limits(self):
        value = config()
        value["nodes"].append(value["nodes"][0])
        with self.assertRaises(ValueError):
            frontend.validate_config(value)
        for streams in (True, 0, -1, 4294967296, "32"):
            value = config()
            value["limits"]["http2_streams_per_connection"] = streams
            with self.assertRaises(ValueError):
                frontend.validate_config(value)
        value = config()
        value["limits"]["visitor_connections"] = True
        with self.assertRaises(ValueError):
            frontend.validate_config(value)

    def test_runtime_map_failure_is_reconciled_on_next_prune(self):
        token = "b" * 32
        frontend.change_claim(self.config, "th1", "claim", token, online=False)
        stale = f"0x1 {frontend.PREFIX}{token} acme_th1"
        with mock.patch.object(frontend.time, "time", return_value=time.time() + frontend.LEASE_SECONDS + 1), \
                mock.patch.object(frontend, "runtime", side_effect=[stale, OSError("socket lost")]):
            with self.assertRaises(OSError):
                frontend.change_claim(self.config, "eu1", "prune")
        self.assertEqual(frontend.MAP.read_text(), "")
        with mock.patch.object(frontend, "runtime", side_effect=[stale, ""]) as runtime:
            frontend.change_claim(self.config, "eu1", "prune")
        self.assertIn("del map", runtime.call_args.args[0])


def unused_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


class Stack:
    def __init__(self, config, synthetic_addresses=False):
        self.temp = tempfile.TemporaryDirectory(prefix="mmx-front-test-")
        self.root = Path(self.temp.name)
        ports = set()
        while len(ports) < 7:
            ports.add(unused_port())
        self.http, self.https, self.plain, self.tls, self.caddy_plain, self.requests, self.admin = list(ports)
        self.logs = []
        self.processes = []
        self.seen = []
        self.release = threading.Event()
        self.entered = threading.Condition()
        self.held = 0
        fixture = self
        seen = self.seen

        class Handler(http.server.BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def handle(self):
                try:
                    super().handle()
                except (BrokenPipeError, ConnectionResetError):
                    pass

            def do_GET(self):
                if self.path == "/hold":
                    with fixture.entered:
                        fixture.held += 1
                        fixture.entered.notify_all()
                    fixture.release.wait(12)
                seen.append(dict(self.headers))
                data = json.dumps(dict(self.headers)).encode()
                self.send_response(200)
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                try:
                    self.wfile.write(data)
                except (BrokenPipeError, ConnectionResetError):
                    pass

            def do_POST(self):
                try:
                    data = self.rfile.read(int(self.headers["Content-Length"]))
                    if len(data) != int(self.headers["Content-Length"]):
                        self.close_connection = True
                        return
                    self.do_GET()
                except (OSError, BrokenPipeError):
                    pass

            def log_message(self, *args):
                pass

        class Server(http.server.ThreadingHTTPServer):
            request_queue_size = 128

        self.backend = Server(("127.0.0.1", 0), Handler)
        self.backend.daemon_threads = True
        threading.Thread(target=self.backend.serve_forever, daemon=True).start()
        subprocess.run(["openssl", "req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256",
                        "-nodes", "-days", "2", "-subj", "/CN=" + config["domain"], "-addext", "subjectAltName=DNS:" + config["domain"],
                        "-keyout", str(self.root / "privkey.pem"), "-out", str(self.root / "fullchain.pem")], check=True, capture_output=True)
        (self.root / "acme.map").touch()
        haproxy = frontend.render_haproxy(config, map_path=self.root / "acme.map", http_port=self.http,
            https_port=self.https, plain_port=self.plain, request_port=self.requests,
            tls_address=f"127.0.0.1:{self.tls}", plain_address=f"127.0.0.1:{self.caddy_plain}",
            rpc_address=f"127.0.0.1:{self.backend.server_port}", socket_path=str(self.root / "admin.sock"))
        haproxy = haproxy.replace("    user haproxy\n", "").replace("    group haproxy\n", "")
        haproxy = haproxy.replace("bind 0.0.0.0:", "bind 127.0.0.1:").replace("bind [::]:", "bind [::1]:")
        if synthetic_addresses:
            # Test-only PROXY input lets real HAProxy admission rules see many
            # IPv6 client addresses without changing the machine's interfaces.
            for port in (self.http, self.https, self.plain):
                haproxy = haproxy.replace(f"bind 127.0.0.1:{port}\n", f"bind 127.0.0.1:{port} accept-proxy\n")
            # Connection rules run before a test PROXY header is decoded. The
            # same admission expressions run at session time in this fixture.
            haproxy = haproxy.replace("tcp-request connection", "tcp-request session")
        (self.root / "haproxy.cfg").write_text(haproxy)
        (self.root / "Caddyfile").write_text(frontend.render_caddy(config, str(self.root),
            tls_port=self.tls, plain_port=self.caddy_plain, request_port=self.requests, admin=f"127.0.0.1:{self.admin}"))
        self.context = ssl.create_default_context(cafile=str(self.root / "fullchain.pem"))
        self.domain = config["domain"]
        self.start([os.environ.get("CADDY", "caddy"), "run", "--config", str(self.root / "Caddyfile"), "--adapter", "caddyfile"])
        self.start(["haproxy", "-db", "-f", str(self.root / "haproxy.cfg")])
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            if any(process.poll() is not None for process in self.processes):
                self.fail_logs()
            try:
                with socket.create_connection(("127.0.0.1", self.caddy_plain), timeout=.1):
                    pass
                with socket.create_connection(("127.0.0.1", self.requests), timeout=.1):
                    return
            except OSError:
                time.sleep(.05)
        self.fail_logs()

    def start(self, command):
        log = tempfile.TemporaryFile()
        self.logs.append(log)
        environment = dict(os.environ, XDG_CONFIG_HOME=str(self.root / "config"), XDG_DATA_HOME=str(self.root / "data"))
        self.processes.append(subprocess.Popen(command, stdout=log, stderr=log, env=environment))

    def fail_logs(self):
        # Capture complete parser diagnostics before terminating other processes.
        time.sleep(.15)
        output = ""
        output += "process return codes: " + str([p.poll() for p in self.processes]) + "\n"
        for log in self.logs:
            log.seek(0)
            output += log.read().decode()
        self.close()
        raise RuntimeError(output)

    def request(self, secure=False, source=None, path="/node/info", headers=None):
        if source:
            connection = http.client.HTTPConnection("127.0.0.1", self.requests, timeout=4)
            headers = dict(headers or {}, **{"X-Mmx-Client": source, "X-Mmx-Scheme": "https"})
        elif secure:
            connection = http.client.HTTPSConnection("127.0.0.1", self.https, context=self.context, timeout=4)
            # Keep a verified hostname while connecting to the isolated loopback port.
            raw = socket.create_connection(("127.0.0.1", self.https), timeout=4)
            connection.sock = self.context.wrap_socket(raw, server_hostname=self.domain)
        else:
            connection = http.client.HTTPConnection("127.0.0.1", self.plain, timeout=4)
        connection.request("GET", path, headers=dict({"Host": self.domain}, **(headers or {})))
        response = connection.getresponse()
        result = response.status, dict(response.getheaders()), response.read()
        connection.close()
        return result

    def close(self):
        self.release.set()
        for process in self.processes:
            process.terminate()
        for process in self.processes:
            process.wait(timeout=5)
        self.backend.shutdown()
        self.backend.server_close()
        for log in self.logs:
            log.close()
        self.temp.cleanup()


@unittest.skipUnless(os.environ.get("CADDY"), "Set CADDY to Caddy >=2.10 with proxy_protocol for integration tests")
class IntegrationTest(unittest.TestCase):
    def setUp(self):
        self.config = config()
        self.stack = None

    def tearDown(self):
        if self.stack:
            self.stack.close()

    def start(self):
        self.stack = Stack(self.config)
        return self.stack

    def test_http2_advertises_32_and_rejects_the_33rd_active_stream(self):
        # Isolate the protocol cap from the separate visitor request budget.
        self.config["limits"]["visitor_connections"] = 64
        stack = self.start()
        verify_http2.verify(stack.domain, "127.0.0.1", stack.https, 32, str(stack.root / "fullchain.pem"))
        with self.assertRaisesRegex(ValueError, "expected 31"):
            verify_http2.verify(stack.domain, "127.0.0.1", stack.https, 31, str(stack.root / "fullchain.pem"))
        stack.context.set_alpn_protocols(["h2"])

        def frame(kind, flags, stream, payload=b""):
            return len(payload).to_bytes(3, "big") + bytes((kind, flags)) + stream.to_bytes(4, "big") + payload

        with socket.create_connection(("127.0.0.1", stack.https), timeout=5) as raw:
            with stack.context.wrap_socket(raw, server_hostname=stack.domain) as connection:
                self.assertEqual(connection.selected_alpn_protocol(), "h2")
                connection.sendall(b"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n" + frame(4, 0, 0))

                def read(count):
                    result = b""
                    while len(result) < count:
                        part = connection.recv(count - len(result))
                        self.assertTrue(part, "HTTP/2 closed before its protocol response")
                        result += part
                    return result

                def receive():
                    header = read(9)
                    return header[3], header[4], int.from_bytes(header[5:], "big"), read(int.from_bytes(header[:3], "big"))

                while True:
                    kind, flags, _, payload = receive()
                    if kind == 4 and not flags & 1:
                        settings = {int.from_bytes(payload[i:i+2], "big"): int.from_bytes(payload[i+2:i+6], "big")
                                    for i in range(0, len(payload), 6)}
                        self.assertEqual(settings.get(3), 32)
                        break
                connection.sendall(frame(4, 1, 0))
                # HPACK: indexed GET/https, literal /hold and authority without
                # dynamic-table mutation, so all requests use the same block.
                authority = stack.domain.encode()
                headers = b"\x82\x87\x04\x05/hold\x01" + bytes((len(authority),)) + authority
                connection.sendall(b"".join(frame(1, 5, stream, headers) for stream in range(1, 64, 2)))
                with stack.entered:
                    self.assertTrue(stack.entered.wait_for(lambda: stack.held == 32, timeout=5), stack.held)
                # Deliberately violate SETTINGS, rather than letting a client
                # library queue the 33rd stream and hide missing enforcement.
                connection.sendall(frame(1, 5, 65, headers))
                connection.settimeout(3)
                while True:
                    kind, _, stream, payload = receive()
                    if kind == 3 and stream == 65:
                        self.assertIn(int.from_bytes(payload, "big"), (1, 7))  # PROTOCOL_ERROR / REFUSED_STREAM
                        break
                    if kind == 7:
                        self.assertEqual(int.from_bytes(payload[4:8], "big"), 1)  # PROTOCOL_ERROR
                        break
                self.assertEqual(stack.held, 32)
        stack.release.set()
        self.assertEqual(stack.request()[0], 200)

    def test_tls_plain_close_and_spoofed_headers(self):
        stack = self.start()
        for secure in (False, True):
            status, headers, body = stack.request(secure=secure, headers={"X-Mmx-Client": "2001:db8::f", "X-Forwarded-For": "198.51.100.9"})
            self.assertEqual(status, 200)
            received = {k.lower(): v for k, v in json.loads(body).items()}
            self.assertEqual(received["x-forwarded-for"], "127.0.0.1")
            self.assertEqual(received["x-forwarded-proto"], "https" if secure else "http")
            self.assertNotIn("x-mmx-client", received)
            if not secure:
                self.assertEqual(headers.get("Connection", headers.get("connection", "")).lower(), "close")
        with socket.create_connection(("127.0.0.1", stack.plain), timeout=3) as connection:
            connection.sendall(b"GET /node/info HTTP/1.1\r\nHost: rpc.mmx.network\r\n\r\n")
            response = b""
            while chunk := connection.recv(65536):
                response += chunk
            self.assertIn(b"200 OK", response)

    def test_acme_routes_to_peer_on_forwarded_http_port(self):
        token, validation = "e" * 32, b"v" * 48
        domain = self.config["domain"]

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                if self.path != frontend.PREFIX + token or self.headers.get("Host") != domain:
                    self.send_error(404)
                    return
                self.send_response(200)
                self.send_header("Content-Length", str(len(validation)))
                self.end_headers()
                self.wfile.write(validation)

            def log_message(self, *args):
                pass

        owner = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        threading.Thread(target=owner.serve_forever, daemon=True).start()
        try:
            peer = next(node for node in self.config["nodes"] if node["id"] == "eu2")
            peer.update(address="127.0.0.1", http_port=owner.server_port)
            stack = self.start()
            with mock.patch.object(frontend, "SOCKET", str(stack.root / "admin.sock")):
                frontend.checked_runtime(f"add map {stack.root / 'acme.map'} {frontend.PREFIX}{token} acme_eu2")
            self.assertEqual(frontend.challenge_get("127.0.0.1", domain, token, stack.http), validation.decode())
        finally:
            owner.shutdown()
            owner.server_close()

    def test_rate_limits_share_ipv6_prefixes_and_return_retry_after(self):
        self.config["limits"].update(visitor_requests_10s=3, prefix48_requests_10s=5)
        stack = self.start()
        for address in ("2001:db8:1:1::1", "2001:db8:1:1::2", "2001:db8:1:1::3"):
            self.assertEqual(stack.request(source=address)[0], 200)
        status, headers, _ = stack.request(source="2001:db8:1:1::ffff")
        self.assertEqual(status, 429)
        self.assertEqual({k.lower(): v for k, v in headers.items()}["retry-after"], "10")
        self.assertEqual(stack.request(source="2001:db8:1:2::1")[0], 200)
        self.assertEqual(stack.request(source="2001:db8:1:3::1")[0], 429)
        self.assertEqual(stack.request(source="2001:db8:2:1::1")[0], 200)

    def test_connection_cap_is_shared_between_plain_and_tls(self):
        self.config["limits"]["visitor_connections"] = 2
        stack = self.start()
        sockets = []
        try:
            for port in (stack.https, stack.plain):
                sockets.append(socket.create_connection(("127.0.0.1", port), timeout=2))
            time.sleep(.1)
            try:
                with socket.create_connection(("127.0.0.1", stack.plain), timeout=2) as excess:
                    excess.sendall(b"GET /node/info HTTP/1.1\r\nHost: rpc.mmx.network\r\n\r\n")
                    self.assertEqual(excess.recv(1024), b"")
            except ConnectionResetError:
                # Admission may reset during connect, send, or receive.
                pass
        finally:
            for connection in sockets:
                connection.close()

    def test_ipv6_64_48_and_total_connection_admission(self):
        self.config["limits"].update(visitor_connections=2, prefix48_connections=3, total_connections=4)
        self.stack = Stack(self.config, synthetic_addresses=True)
        stack = self.stack
        opened = []

        def accepted(address):
            connection = socket.create_connection(("127.0.0.1", stack.https), timeout=2)
            opened.append(connection)
            connection.sendall(f"PROXY TCP6 {address} ::1 12345 443\r\n".encode())
            connection.settimeout(.15)
            try:
                return bool(connection.recv(1))
            except socket.timeout:
                return True
            except ConnectionResetError:
                return False

        try:
            self.assertTrue(accepted("2001:db8:1:1::1"))
            self.assertTrue(accepted("2001:db8:1:1::2"))
            self.assertFalse(accepted("2001:db8:1:1::3"))  # /64
            self.assertTrue(accepted("2001:db8:1:2::1"))
            self.assertFalse(accepted("2001:db8:1:3::1"))  # /48
            self.assertTrue(accepted("2001:db8:2:1::1"))
            self.assertFalse(accepted("2001:db8:3:1::1"))  # node total
        finally:
            for connection in opened:
                connection.close()

    def test_body_has_absolute_five_second_deadline_despite_trickling_bytes(self):
        stack = self.start()
        for secure in (False, True):
            connection = socket.create_connection(("127.0.0.1", stack.https if secure else stack.plain), timeout=8)
            if secure:
                connection = stack.context.wrap_socket(connection, server_hostname=stack.domain)
            with connection:
                connection.sendall(b"POST /test HTTP/1.1\r\nHost: rpc.mmx.network\r\nContent-Length: 100\r\n\r\nx")
                started = time.monotonic()
                response = b""
                for _ in range(7):
                    time.sleep(1)
                    try:
                        connection.sendall(b"x")
                    except (BrokenPipeError, ConnectionResetError):
                        break
                    connection.settimeout(.01)
                    try:
                        chunk = connection.recv(65536)
                        response += chunk
                        if not chunk or response:
                            break
                    except socket.timeout:
                        pass
                self.assertLess(time.monotonic() - started, 6.5)
                self.assertNotIn(b"200 OK", response)
                self.assertEqual(stack.seen, [])

    def test_http2_body_deadline(self):
        stack = self.start()
        body = stack.root / "body.txt"
        body.write_bytes(b"x" * 100)
        result = subprocess.run(["curl", "--silent", "--show-error", "--http2", "--noproxy", "*",
            "--resolve", f"{stack.domain}:{stack.https}:127.0.0.1", "--cacert", str(stack.root / "fullchain.pem"),
            "--limit-rate", "1", "--max-time", "8", "--data-binary", "@" + str(body),
            "--output", "/dev/null", "--write-out", "%{http_version} %{http_code} %{time_total}",
            f"https://{stack.domain}:{stack.https}/test"], capture_output=True, text=True, timeout=10)
        version, status, elapsed = result.stdout.strip().split()
        self.assertEqual(version, "2", result.stderr)
        self.assertNotEqual(status, "200")
        self.assertLess(float(elapsed), 7, result.stderr)
        self.assertEqual(stack.seen, [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
