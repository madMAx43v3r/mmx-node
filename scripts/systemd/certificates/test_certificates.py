#!/usr/bin/python3
"""Exercise archive restrictions, real certificate validation, and rollback."""

import importlib.util
import io
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "receiver", Path(__file__).with_name("install-rpc-certificate.py"))
receiver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(receiver)


def archive(entries):
    output = io.BytesIO()
    with tarfile.open(fileobj=output, mode="w:") as tar:
        for name, content, kind in entries:
            entry = tarfile.TarInfo(name)
            entry.type = kind
            entry.size = len(content)
            tar.addfile(entry, io.BytesIO(content))
    output.seek(0)
    return output


class Certificates(unittest.TestCase):
    def test_archive_restrictions(self):
        regular = tarfile.REGTYPE
        good = [("fullchain.pem", b"cert", regular), ("privkey.pem", b"key", regular)]
        self.assertEqual(receiver.read_bundle(archive(good)),
                         {"fullchain.pem": b"cert", "privkey.pem": b"key"})
        for bad in [good[:1], good + good[:1], good + [("../escape", b"", regular)],
                    [("fullchain.pem", b"", tarfile.SYMTYPE), good[1]]]:
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                receiver.read_bundle(archive(bad))
        with self.assertRaises(ValueError):
            receiver.read_bundle(io.BytesIO(b"x" * (receiver.MAX_BUNDLE + 1)))

    def test_atomic_activation_and_rollback(self):
        with tempfile.TemporaryDirectory() as path:
            root = Path(path)
            (root / "old").mkdir()
            (root / "new").mkdir()
            (root / "current").symlink_to("old")
            calls = []
            def fail():
                calls.append(os.readlink(root / "current"))
                raise ValueError("reload failed")
            with self.assertRaises(ValueError):
                receiver.activate(root, "new", reload=fail)
            self.assertEqual(os.readlink(root / "current"), "old")
            self.assertEqual(calls, ["new", "old"])
            self.assertEqual(receiver.activate(root, "new", reload=lambda: None), "updated")
            self.assertEqual(receiver.activate(root, "new", reload=fail), "unchanged")
            with self.assertRaises(ValueError):
                receiver.activate(root, "old", reload=lambda: None,
                                  check=lambda: (_ for _ in ()).throw(ValueError("TLS mismatch")))
            self.assertEqual(os.readlink(root / "current"), "new")

    def test_real_certificate_validation(self):
        def openssl(*args):
            subprocess.run(["openssl", *args], check=True, capture_output=True)
        with tempfile.TemporaryDirectory() as path:
            root = Path(path)
            openssl("req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256",
                    "-nodes", "-days", "30", "-subj", "/CN=Test CA",
                    "-addext", "basicConstraints=critical,CA:TRUE",
                    "-keyout", str(root / "ca.key"), "-out", str(root / "ca.pem"))
            openssl("req", "-new", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256",
                    "-nodes", "-subj", "/CN=rpc.mmx.network",
                    "-keyout", str(root / "privkey.pem"), "-out", str(root / "leaf.csr"))
            extensions = root / "extensions"
            extensions.write_text("subjectAltName=DNS:rpc.mmx.network\nextendedKeyUsage=serverAuth\n")
            def sign(days):
                openssl("x509", "-req", "-in", str(root / "leaf.csr"),
                        "-CA", str(root / "ca.pem"), "-CAkey", str(root / "ca.key"),
                        "-CAcreateserial", "-days", str(days), "-extfile", str(extensions),
                        "-out", str(root / "fullchain.pem"))
            sign(20)
            self.assertTrue(receiver.validate(root, cafile=root / "ca.pem"))
            with self.assertRaises(subprocess.CalledProcessError):
                receiver.validate(root)  # The system trust store rejects the test CA.
            original = (root / "privkey.pem").read_bytes()
            (root / "privkey.pem").write_bytes((root / "ca.key").read_bytes())
            with self.assertRaises(ValueError):
                receiver.validate(root, cafile=root / "ca.pem")
            (root / "privkey.pem").write_bytes(original)
            extensions.write_text("subjectAltName=DNS:other.example\nextendedKeyUsage=serverAuth\n")
            sign(20)
            with self.assertRaises(subprocess.CalledProcessError):
                receiver.validate(root, cafile=root / "ca.pem")
            extensions.write_text("subjectAltName=DNS:rpc.mmx.network\nextendedKeyUsage=serverAuth\n")
            sign(1)
            with self.assertRaises(subprocess.CalledProcessError):
                receiver.validate(root, cafile=root / "ca.pem")


if __name__ == "__main__":
    unittest.main()
