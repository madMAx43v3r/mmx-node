#!/usr/bin/env python3
"""Linux CLI contract tests. Uses disposable wallets and local mock RPC only.

Run: python3 test/test_mmxwallet_cli.py --binary /absolute/path/to/mmxwallet
"""
import argparse
import http.server
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile
import threading
import unittest
from urllib.parse import parse_qs, urlsplit

BINARY = None

class RPC(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def reply(self, data, status=200):
        body = b'' if status == 204 else data if isinstance(data, bytes) else json.dumps(data).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = urlsplit(self.path)
        query = parse_qs(path.query)
        if path.path in self.server.responses:
            body, status = self.server.responses[path.path]
            return self.reply(body, status)
        if path.path == '/chain/info':
            self.reply({'network': 'mainnet', 'decimals': 6})
        elif path.path == '/node/info':
            self.reply({'name': 'mainnet', 'height': self.server.height, 'is_synced': True})
        elif path.path == '/address':
            self.reply({'balances': [{'contract': self.server.currency, 'amount': self.server.amount, 'decimals': 6, 'symbol': 'MMX'}]})
        elif path.path == '/address/history':
            self.reply([{'contract': self.server.currency, 'amount': '1234567', 'decimals': 6,
                         'symbol': 'MMX', 'address': query['id'][0], 'txid': 'a' * 64,
                         'is_pending': False, 'height': 990, 'time_stamp': 1700000000000,
                         'type': 'RECEIVE', 'memo': 'hello <b>世界</b>\x00\x01\n'}])
        elif path.path == '/transaction':
            self.reply(self.server.transaction, 204 if self.server.transaction is None else 200)
        else:
            self.reply({'error': 'unknown endpoint'}, 404)

    def do_POST(self):
        body = self.rfile.read(int(self.headers['Content-Length']))
        self.server.posts.append((self.path, body))
        if self.path == '/transaction/validate':
            self.reply({'did_fail': False, 'total_fee': str(self.server.fee)})
        elif self.path == '/transaction/broadcast':
            self.reply({})
        else:
            self.reply({}, 404)

class WalletCLI(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixtures = tempfile.TemporaryDirectory(prefix='mmxwallet-fixtures-')
        cls.fixture = Path(cls.fixtures.name)
        env = dict(os.environ, MMX_HOME=str(cls.fixture))
        proc = subprocess.run([BINARY, 'create', '--json', '--show-mnemonic'], cwd=cls.fixture,
                              env=env, text=True, capture_output=True, timeout=30)
        if proc.returncode:
            raise RuntimeError('fixture wallet creation failed: ' + proc.stderr)
        cls.wallet = json.loads(proc.stdout)
        cls.server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), RPC)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.url = 'http://127.0.0.1:' + str(cls.server.server_port)
        # Native currency address (Bech32m encoded zero hash), obtained from wallet output.
        # Fixed native address from the chain's Bech32m encoding.
        cls.server.currency = 'mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev'

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()
        cls.fixtures.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='mmxwallet-test-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.env = dict(os.environ, HOME=str(self.root), MMX_HOME=str(self.root / 'wallets'))
        self.file = self.root / "wallet '$(touch injected)' 世界.dat"
        shutil.copyfile(self.wallet['wallet_file'], self.file)
        self.server.posts = []
        self.server.responses = {}
        self.server.height = 1000
        self.server.fee = 100
        self.server.amount = '9007199254740993'
        self.server.transaction = None

    def run_cli(self, *args, secret=None, error=None, env=None, json_mode=True):
        argv = [BINARY, *args]
        if json_mode:
            argv.append('--json')
        if secret is not None:
            argv.append('--input-stdin')
        payload = secret if isinstance(secret, str) else json.dumps(secret) + '\n' if secret is not None else ''
        proc = subprocess.run(argv, input=payload, cwd=self.root, env=self.env if env is None else env,
                              text=True, capture_output=True, timeout=30)
        if error:
            self.assertNotEqual(proc.returncode, 0)
            self.assertEqual(proc.stdout, '')
            result = json.loads(proc.stderr)
            self.assertEqual(result['code'], error)
            self.assertEqual(result['status'], 'error')
        else:
            self.assertEqual(proc.returncode, 0, proc.stderr)
            self.assertEqual(proc.stderr, '')
            if not json_mode:
                return proc.stdout
            result = json.loads(proc.stdout)
        self.assertEqual(result['schema_version'], 1)
        self.assertEqual(result['command'], args[0])
        return result

    def test_capabilities_and_empty_list(self):
        caps = self.run_cli('capabilities')
        self.assertEqual(caps['memo_max_bytes'], 64)
        self.assertEqual(caps['secret_input'], 'stdin-json-line')
        self.assertIn('transaction', caps['commands'])
        self.assertEqual(self.run_cli('list')['wallets'], [])

    def test_create_import_addresses_and_permissions(self):
        created = self.run_cli('create')
        self.assertNotIn('mnemonic', created)
        self.assertEqual(stat.S_IMODE(Path(created['wallet_file']).stat().st_mode), 0o600)
        self.assertEqual(stat.S_IMODE(Path(created['wallet_file']).parent.stat().st_mode), 0o700)
        words = self.run_cli('get', 'mnemonic')['mnemonic']
        imported = self.run_cli('import', '--file', str(self.root / 'restored.dat'), secret={'mnemonic': words})
        self.assertEqual(created['address'], imported['address'])
        addresses = self.run_cli('addresses', '--num-addresses', '3')['addresses']
        self.assertEqual(len(addresses), 3)
        self.assertEqual(addresses[0], created['address'])
        self.assertEqual(self.run_cli('address', '--num-addresses', '3', '--offset', '2')['address'], addresses[2])
        listed = self.run_cli('list')['wallets'][0]
        self.assertEqual(listed['active'], True)
        self.assertEqual(listed['address'], created['address'])
        self.assertIn(created['address'], self.run_cli('list', json_mode=False))
        account_address = self.run_cli('address', '--account', '7')['address']
        self.assertEqual(self.run_cli('list', '--account', '7')['wallets'][0]['address'], account_address)

    def test_existing_wallet_not_overwritten(self):
        before = self.file.read_bytes()
        self.run_cli('create', '--file', str(self.file), error='wallet_exists')
        self.assertEqual(before, self.file.read_bytes())

    def test_selection_and_custom_file(self):
        created = self.run_cli('create')
        second = self.run_cli('create')
        wallets = self.run_cli('list')['wallets']
        self.assertEqual({w['fingerprint']: w['address'] for w in wallets},
                         {created['fingerprint']: created['address'], second['fingerprint']: second['address']})
        index = next(w['index'] for w in wallets if w['fingerprint'] == created['fingerprint'])
        self.run_cli('use', str(index))
        self.assertEqual(self.run_cli('address')['address'], created['address'])
        self.assertEqual(self.run_cli('address', '--wallet', second['fingerprint'])['address'], second['address'])
        self.assertEqual(self.run_cli('address')['address'], created['address'])
        self.assertEqual(self.run_cli('address', '--file', str(self.file))['address'], self.wallet['address'])
        self.assertFalse((self.root / 'injected').exists())

    def test_passphrase_input_never_prompts(self):
        created = self.run_cli('create', '--with-passphrase', secret={'passphrase': 'temporary test secret 世界 🔑 \\u1234'})
        self.assertTrue(created['with_passphrase'])
        self.assertIsNone(self.run_cli('list')['wallets'][0]['address'])
        self.assertIn('[passphrase required]', self.run_cli('list', json_mode=False))
        self.assertIsNone(self.run_cli('list', secret={'passphrase': 'wrong'})['wallets'][0]['address'])
        self.assertEqual(self.run_cli('list', secret={'passphrase': 'temporary test secret 世界 🔑 \\u1234'})['wallets'][0]['address'], created['address'])
        empty = self.run_cli('create', '--with-passphrase', '--file', str(self.root / 'empty-passphrase.dat'), secret={'passphrase': ''})
        self.assertTrue(empty['with_passphrase'])
        self.assertEqual(self.run_cli('address', '--file', empty['wallet_file'], secret={'passphrase': ''})['address'], empty['address'])
        self.run_cli('address', error='passphrase_required')
        self.run_cli('address', secret={'passphrase': 'wrong'}, error='invalid_passphrase')
        self.assertEqual(self.run_cli('address', secret={'passphrase': 'temporary test secret 世界 🔑 \\u1234'})['address'], created['address'])

    def test_import_and_secret_input_errors_are_redacted(self):
        self.run_cli('import', error='mnemonic_required')
        invalid = self.run_cli('import', secret={'mnemonic': 'never-echo-this-secret'}, error='invalid_mnemonic')
        self.assertNotIn('never-echo-this-secret', invalid['error'])
        invalid = self.run_cli('import', secret='never-echo-this-secret\n', error='invalid_secret_input')
        self.assertNotIn('never-echo-this-secret', invalid['error'])
        self.run_cli('import', secret='{' + '"passphrase":"' + r'\ud800' + '"}\n', error='invalid_secret_input')
        self.run_cli('import', secret='{} garbage\n', error='invalid_secret_input')
        self.run_cli('import', secret={'passphrase': 42}, error='invalid_secret_input')
        self.run_cli('import', secret='x' * 16385 + '\n', error='invalid_secret_input')

    def test_missing_home_does_not_write_cwd(self):
        env = dict(self.env)
        env.pop('HOME', None)
        env.pop('MMX_HOME', None)
        self.run_cli('create', env=env, error='wallet_directory_unavailable')
        self.assertEqual(self.run_cli('address', '--file', str(self.file), env=env)['address'], self.wallet['address'])
        self.run_cli('capabilities', env=env)

    def test_normal_cli_output_preserved(self):
        self.assertEqual(self.run_cli('address', '--file', str(self.file), json_mode=False).strip(), self.wallet['address'])
        self.assertEqual(self.run_cli('get', 'mnemonic', '--file', str(self.file), json_mode=False).strip(), self.wallet['mnemonic'])

    def test_rpc_balance_history_and_info(self):
        info = self.run_cli('info', '--rpc', self.url)
        self.assertTrue(info['is_synced'])
        balance = self.run_cli('balance', '--file', str(self.file), '--rpc', self.url)
        self.assertEqual(balance['balances'][0]['amount_atomic'], '9007199254740993')
        history = self.run_cli('history', '--file', str(self.file), '--rpc', self.url)
        self.assertEqual(history['history'][0]['amount_atomic'], '1234567')
        self.assertEqual(history['history'][0]['memo'], 'hello <b>世界</b>\x00\x01\n')

    def test_full_width_atomic_balance_is_formatted_exactly(self):
        self.server.amount = '340282366920938463463374607431768211455'
        row = self.run_cli('balance', '--file', str(self.file), '--rpc', self.url)['balances'][0]
        self.assertEqual(row['amount_atomic'], self.server.amount)
        self.assertEqual(row['amount'], '340282366920938463463374607431768.211455')

    def test_native_http_without_path_and_ignore_curlrc(self):
        env = dict(self.env, PATH='')
        (self.root / '.curlrc').write_text('url = "http://127.0.0.1:1"\noutput = "injected"\n')
        self.run_cli('info', '--rpc', self.url, env=env)
        self.assertFalse((self.root / 'injected').exists())
        caps = self.run_cli('capabilities')
        self.assertFalse(caps['curl_override'])
        self.assertEqual(caps['http_transport'], 'native-rust')
        self.run_cli('info', '--curl', '/missing/curl', '--rpc', self.url, error='invalid_argument')

    def test_native_http_never_starts_curl_or_uses_request_files(self):
        fake = self.root / 'curl'
        fake.write_text('#!/bin/sh\ntouch "' + str(self.root / 'injected') + '"\nexit 1\n')
        fake.chmod(0o700)
        env = dict(self.env, PATH=str(self.root), TMPDIR=str(self.root))
        self.run_cli('info', '--rpc', self.url, env=env)
        self.assertFalse((self.root / 'injected').exists())
        self.assertFalse(any(self.root.glob('mmxwallet-*')))
        self.run_cli('info', '--rpc', 'ftp://127.0.0.1', error='invalid_argument')

    def test_rpc_errors_are_structured(self):
        self.server.responses['/node/info'] = ({'error': 'busy'}, 503)
        self.run_cli('info', '--rpc', self.url, error='rpc_http_error')
        self.server.responses['/node/info'] = (b'{"name":', 200)
        self.run_cli('info', '--rpc', self.url, error='rpc_response_invalid')
        self.server.responses['/node/info'] = ({'name': 'mainnet', 'height': 1000, 'is_synced': False}, 200)
        self.run_cli('balance', '--file', str(self.file), '--rpc', self.url, error='rpc_not_synced')
        self.server.responses['/node/info'] = ({'name': 'other-network', 'height': 1000, 'is_synced': True}, 200)
        self.run_cli('balance', '--file', str(self.file), '--rpc', self.url, error='rpc_network_mismatch')

    def prepare(self, memo=None, amount='1.234567'):
        path = self.root / "signed ' 世界.json"
        args = ['send', '--file', str(self.file), '--rpc', self.url, '--target', self.wallet['address'],
                '--amount', amount, '--transaction', str(path)]
        if memo is not None:
            args += ['--memo', memo]
        return self.run_cli(*args), path

    def test_prepare_and_broadcast_same_bytes(self):
        result, path = self.prepare('memo 世界')
        self.assertEqual(result['amount_atomic'], '1234567')
        self.assertFalse(result['broadcast'])
        self.assertEqual(result['status'], 'validated')
        self.assertEqual(result['memo'], 'memo 世界')
        self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)
        self.assertFalse(any(endpoint.endswith('/broadcast') for endpoint, _ in self.server.posts))
        signed = path.read_bytes()
        result2 = self.run_cli('broadcast', '--transaction', str(path), '--rpc', self.url)
        self.assertEqual(result['transaction_id'], result2['transaction_id'])
        self.assertEqual([body for endpoint, body in self.server.posts if endpoint.endswith('/broadcast')], [signed])

    def test_payment_amounts_are_exact_and_never_rounded(self):
        for number, atomic in [('0.000001', '1'), ('1e-6', '1'), ('1.000001', '1000001'),
                               ('900719925.474099', '900719925474099'), ('1.2300000', '1230000')]:
            with self.subTest(number=number):
                result, path = self.prepare(amount=number)
                self.assertEqual(result['amount_atomic'], atomic)
                path.unlink()
        for number in ['1.0000001', '0.0000009', '-1', '1e999', '340282366920938463463374607431768211456']:
            with self.subTest(number=number):
                self.run_cli('send', '--file', str(self.file), '--rpc', self.url,
                             '--target', self.wallet['address'], '--amount', number, error='invalid_amount')

    def test_prepared_transaction_is_not_overwritten(self):
        _, path = self.prepare()
        before = path.read_bytes()
        self.run_cli('send', '--file', str(self.file), '--rpc', self.url, '--target', self.wallet['address'],
                     '--amount', '2', '--transaction', str(path), error='transaction_exists')
        self.assertEqual(before, path.read_bytes())

    def test_send_without_memo(self):
        result, _ = self.prepare()
        self.assertNotIn('memo', result)
        self.assertFalse(result['broadcast'])

    def test_explicit_yes_broadcasts(self):
        result = self.run_cli('send', '--file', str(self.file), '--rpc', self.url,
                              '--target', self.wallet['address'], '--amount', '1', '--yes')
        self.assertTrue(result['broadcast'])
        self.assertEqual(result['status'], 'broadcast')
        self.assertEqual(sum(endpoint.endswith('/broadcast') for endpoint, _ in self.server.posts), 1)

    def test_no_network_side_effect_after_failed_save(self):
        missing = self.root / 'nonexistent' / 'signed.json'
        self.run_cli('send', '--file', str(self.file), '--rpc', self.url,
                     '--target', self.wallet['address'], '--amount', '1', '--transaction', str(missing),
                     '--yes', error='io_error')
        self.assertFalse(any(endpoint.endswith('/broadcast') for endpoint, _ in self.server.posts))

    def test_utf8_memo_limit(self):
        self.prepare('é' * 32)
        self.run_cli('send', '--file', str(self.file), '--rpc', self.url, '--target', self.wallet['address'],
                     '--amount', '1', '--memo', 'é' * 33, error='invalid_argument')

    def test_expired_or_excess_fee_broadcast_rejected(self):
        _, path = self.prepare()
        self.server.fee = 2**63
        self.run_cli('broadcast', '--transaction', str(path), '--rpc', self.url, error='rpc_response_invalid')
        self.server.fee = 100
        self.server.height = 1101
        self.run_cli('broadcast', '--transaction', str(path), '--rpc', self.url, error='transaction_expired')
        self.assertFalse(any(endpoint.endswith('/broadcast') for endpoint, _ in self.server.posts))

    def test_tampered_signature_and_old_transaction_are_rejected(self):
        _, path = self.prepare()
        original = json.loads(path.read_bytes())
        self.assertEqual(original['version'], 1)
        for field in ['signature', 'pubkey']:
            changed = json.loads(json.dumps(original))
            value = changed['solutions'][0][field]
            changed['solutions'][0][field] = ('0' if value[0] != '0' else '1') + value[1:]
            path.write_text(json.dumps(changed))
            self.run_cli('broadcast', '--transaction', str(path), '--rpc', self.url, error='wallet_error')
        original['version'] = 0
        path.write_text(json.dumps(original))
        self.run_cli('broadcast', '--transaction', str(path), '--rpc', self.url, error='wallet_error')
        self.assertFalse(any(endpoint.endswith('/broadcast') for endpoint, _ in self.server.posts))

    def test_insufficient_funds_is_structured(self):
        self.server.amount = '1'
        self.run_cli('send', '--file', str(self.file), '--rpc', self.url,
                     '--target', self.wallet['address'], '--amount', '1', error='insufficient_funds')
        self.assertEqual(self.server.posts, [])

    def test_rpc_redirect_is_not_followed(self):
        self.server.responses['/node/info'] = ({}, 302)
        self.run_cli('info', '--rpc', self.url, error='rpc_http_error')

    def test_transaction_status_unknown_pending_included_failed(self):
        txid = 'a' * 64
        result = self.run_cli('transaction', txid, '--rpc', self.url)
        self.assertEqual(result['status'], 'unknown')
        self.server.transaction = {'id': txid, 'height': None, 'expires': 1100, 'did_fail': False}
        self.assertEqual(self.run_cli('transaction', txid, '--rpc', self.url)['status'], 'pending')
        self.server.transaction['expires'] = 999
        self.assertEqual(self.run_cli('transaction', txid, '--rpc', self.url)['status'], 'expired')
        self.server.transaction['height'] = 995
        included = self.run_cli('transaction', txid, '--rpc', self.url)
        self.assertEqual(included['status'], 'included')
        self.assertEqual(included['confirmations'], 6)
        self.server.transaction['did_fail'] = True
        self.assertEqual(self.run_cli('transaction', txid, '--rpc', self.url)['status'], 'failed')
        self.server.transaction['id'] = 'b' * 64
        self.run_cli('transaction', txid, '--rpc', self.url, error='rpc_response_invalid')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', required=True)
    args, remaining = parser.parse_known_args()
    BINARY = str(Path(args.binary).resolve())
    unittest.main(argv=[__file__, *remaining])
