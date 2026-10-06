---
title: CLI Commands
description: MMX Node CLI Command Reference.
---

When compiled from source:
```bash frame="none"
cd mmx-node
source ./activate.sh
```
With a binary package install, just open a new terminal. On Windows search for `MMX CMD`.

To run any `mmx` commands (except `mmx wallet create`), the node needs to be running. See [Getting Started](../../guides/getting-started/) to read on how to start it.

## Standalone Wallet CLI

`mmxwallet` keeps the seed and signs transactions locally, but gets chain state and submits signed transactions through
the public RPC. It does not need a local MMX node. The default RPC is `rpc.mmx.network`; use `--rpc <URL>` to select
another public RPC.

To create a wallet: `mmxwallet create [--with-passphrase]`

To import a wallet from its mnemonic seed phrase: `mmxwallet import [--with-passphrase]`

Wallet files use the standard `wallet_<fingerprint>.dat` naming convention.
Creating or importing a wallet makes it active. List wallets with `mmxwallet list`, then persistently select the active
wallet by list index or fingerprint with `mmxwallet use <index|fingerprint>`. The selection is stored in
`mmxwallet.json`.

`list` also shows each wallet's first address for the selected `--account` (default 0). Passphrase wallets show
`[passphrase required]` in text output, or `address: null` in JSON, until a matching passphrase is supplied with
`--input-stdin`. Listing never prompts, and wallets with other passphrases remain listed with unavailable addresses.

Use `--wallet <fingerprint>` with any wallet command to select a wallet for that invocation without changing the active
wallet. One-shot selection deliberately does not accept list indices. For example:
`mmxwallet send --wallet <fingerprint> --target <address> --amount <value>`.

To show the first address: `mmxwallet address`

To show every derived address: `mmxwallet addresses --num-addresses <count>`

To show the primary address and mnemonic seed phrase: `mmxwallet mnemonic`

To print only the raw word list: `mmxwallet get mnemonic`

To show the MMX balance: `mmxwallet balance`

To show a token balance: `mmxwallet balance --currency <token_address|symbol>`

To show all fungible currency balances: `mmxwallet balance --currency all`

NFT holdings are excluded from these currency balances and do not block MMX or token operations.

To show recent transaction history: `mmxwallet history [--limit <count>]`

History shows only MMX by default. Use `--currency <token_address|symbol>` to show another currency, or `--currency all`
to show all currencies. Every currency with an exactly matching symbol is included. History defaults to 20 entries and
accepts a limit from 1 to 1000. Entries are printed oldest first, with the latest entry last. Use
`--num-addresses <count>` to include additional derived addresses, up to a hard limit of 10.

To transfer MMX: `mmxwallet send --target <address> --amount <value>`

To transfer a token: `mmxwallet send --target <address> --amount <value> --currency <token_address>`

To check the configured public RPC: `mmxwallet info`

By default wallets are stored in `~/.mmx/wallet/`, or `$MMX_HOME/wallet/` when `MMX_HOME` is set. Existing `wallet.dat` and
`wallet_<fingerprint>.dat` node-wallet files are also discovered, and new files use `wallet_<fingerprint>.dat`.
Use `--file <path>` to select another key file directly. The key file format remains compatible
with the existing MMX wallet and GUI.

The optional passphrase changes key derivation; it does not encrypt the seed stored in the wallet file. Keep the key
file and the mnemonic backup private. Only public addresses and signed transactions are sent to the RPC. HTTP(S)
uses an in-process Rust client with certificate verification; curl is not required.

### Desktop and automation interface

On Linux, `--json` emits one JSON object on stdout on success and one JSON error object on stderr on failure
(exit status 1). Each object includes `schema_version: 1`, `command`, and `status`. Machine mode never prompts;
human-readable output remains the default. Help output is intended for humans.

`mmxwallet capabilities --json` reports the supported commands, schema version, secret input mechanism,
maximum memo byte length, maximum address count, and transaction-preparation support. Wallet commands supported
in JSON mode are `create`, `import`, `list`, `use`, `mnemonic`, `get mnemonic`, `address`, `addresses`, `balance`,
`history`, `send`, `broadcast`, `info`, and `transaction`.

Supply secrets through a private stdin pipe using `--input-stdin`. It reads one JSON line, up to 16 KiB,
containing optional `mnemonic` and `passphrase` string fields. It accepts normal UTF-8 and JSON Unicode escapes.
Do not put recovery words or passphrases in command arguments or environment variables. For example, a desktop
controller starts `mmxwallet import --json --input-stdin --file /chosen/wallet.dat` and writes this shape to stdin:

```json
{"mnemonic":"<recovery words>","passphrase":"<optional derivation passphrase>"}
```

Pass `--with-passphrase` when creating/importing a wallet that should use the supplied derivation passphrase.
For an existing passphrase wallet, `address`, `addresses`, `balance`, and `history` also need its passphrase to
derive addresses. Missing input returns `mnemonic_required` or `passphrase_required`; wrong input returns
`invalid_mnemonic` or `invalid_passphrase`. Malformed secret input returns `invalid_secret_input` without echoing it.
An empty passphrase can be supplied explicitly. Passphrases affect derivation and do not encrypt the key file.

`create` and `import` JSON omit recovery words unless `--show-mnemonic` is explicitly requested. The dedicated
`mnemonic` / `get mnemonic` commands return them intentionally. Treat these outputs as secrets and exclude them
from application logs. `--non-interactive` can also disable prompts without selecting JSON output.

| Command | JSON result fields, in addition to the envelope |
| --- | --- |
| `create`, `import` | Absolute `wallet_file`, `fingerprint`, primary `address`, `with_passphrase`; optional `mnemonic` |
| `list` | Absolute `wallet_directory`, `wallets` array with `index`, `fingerprint`, absolute `wallet_file`, `with_passphrase`, `active`, first `address` (null when the passphrase is unavailable) |
| `use` | Selected `fingerprint` and absolute `wallet_file` |
| `address`, `addresses` | Absolute `wallet_file`, `address` or `addresses` array |
| `mnemonic`, `get mnemonic` | `mnemonic`, absolute `wallet_file`, `fingerprint` |
| `balance` | `network`, `current_height`, `balances` array with `currency_address`, `symbol`, `decimals`, decimal-string `amount`, integer-string `amount_atomic` |
| `history` | `network`, `current_height`, newest-first `history` array; RPC entry fields preserved, `amount` formatted as a decimal string and `amount_atomic` as an integer string |
| `info` | `rpc`, `network`, `height`, `is_synced` |
| `send` | Existing transfer fields, plus `network`, `decimals`, integer-string `max_fee_atomic`, `transaction_file` |
| `broadcast` | `transaction_id`, `broadcast: true`, `status: "broadcast"` |
| `transaction` | `transaction_id`, `network`, `current_height`, `confirmations`, `status`, raw `transaction` or null |

Keep amounts as strings or arbitrary-precision integers. Payment amounts are converted directly from decimal
argument digits to atomic units, including scientific notation; overflow and fractional atomic units are
rejected with `invalid_amount` instead of being rounded. Memo limits are **64 UTF-8 bytes**, not 64 characters.
The signed maximum fee includes all distinct sender/input signatures plus the requested gas allowance.
Fee-payer affordability is checked by RPC validation; the local wallet still checks funds for transfer inputs.

### Prepare, review, and broadcast

A GUI should use two separate operations:

```sh
mmxwallet send --json --file /chosen/wallet.dat --target <address> --amount 1.234567 --memo "optional memo" --transaction /private/new-transaction.json
mmxwallet broadcast --json --transaction /private/new-transaction.json
```

Without `--yes`, JSON/noninteractive send signs and validates but does not broadcast. It returns `status: "validated"`,
`broadcast: false`, the transaction ID, exact amount/fee, signed maximum fee, and expiry/current heights.
Save the transaction before presenting its review. `broadcast` validates and submits the same saved bytes;
it refuses an expired transaction or a validation fee above its signed maximum. Linux saves wallet and
transaction files privately, syncs their contents/directory, and refuses to overwrite an existing file.
Use a fresh transaction path for each preparation; an existing path returns `transaction_exists`.
A failed save does not broadcast, including when `--yes` was supplied.

`--yes` retains the direct-send behavior for callers that explicitly want it. No automatic payment retry is
performed. If the RPC times out or fails during submission, it may have accepted the transaction already.
Check the saved transaction ID before deciding whether to rebroadcast the same bytes.

```sh
mmxwallet transaction <64-character-hex-txid> --json
```

Statuses are `unknown`, `pending`, `expired`, `included`, or `failed`. Included/failed transactions include a
confirmation count. `unknown` means the selected RPC does not currently know the transaction; it does not
prove that the transaction failed or expired. Poll again to observe confirmation changes or reorganizations.
The command needs no wallet key or passphrase.

### Native HTTP(S) transport

The CLI uses ureq and rustls for HTTP(S), with bundled CA roots, a 30-second request timeout, a 10-second
connection timeout, a 16 MiB response limit, and no redirects or automatic payment retries. No external process
or temporary request/response files are used. `--curl` has been removed; capabilities now reports
`curl_override: false` and `http_transport: "native-rust"`.
Errors include `rpc_timeout`, `rpc_transport_error`, `rpc_http_error`, `rpc_response_invalid`,
`rpc_not_synced`, and `rpc_network_mismatch`. `insufficient_funds` remains the wallet liquidity error.

Wallet location defaults are `$MMX_HOME/wallet/` or `$HOME/.mmx/wallet/`. Empty environment values are ignored. If neither
is available, directory-based operations fail with `wallet_directory_unavailable` instead of writing to the
current directory. An explicit `--file` and wallet-independent RPC commands do not need either variable.

### Rust crates and builds

The root Cargo workspace contains two packages:

- `rust/mmx-wallet` (`mmx-wallet`): reusable wallet library with key-file encoding/decoding, mnemonic conversion,
  fingerprinting, account/address derivation, exact amounts, and offline transfer signing/verification.
  It has no filesystem policy, CLI state, HTTP client, or C++ bindings.
- `rust/mmxwallet` (`mmxwallet`): CLI arguments, secret prompts/stdin input, wallet discovery/selection,
  durable private saves, and HTTP RPC operations.

Transactions use hardfork2 version 1 only. Version 0 saved transactions are rejected. Existing MMX seeds,
mnemonics, and VNX `.dat` key files remain compatible. The wallet supports simple MMX/token transfers;
contract deployment and execution are outside this CLI.

Build and test with Rust 1.85 or newer:

```sh
cargo build --locked --release -p mmxwallet
cargo test --locked --workspace
python3 test/test_mmxwallet_cli.py --binary target/release/mmxwallet -v
```

CMake detects Rust and Cargo, adds the `mmxwallet` target when both are usable, and installs the resulting binary
into `bin`. If either is missing or the toolchain is unavailable, CMake skips the wallet build and installation
with a status message. There is no C++ wallet fallback. To build it without
configuring or compiling the C++ node and its dependencies:

```sh
cmake -S . -B build-wallet -DMMX_WALLET_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-wallet --target mmxwallet
```

The Rust compatibility tests use fixed, disposable-seed vectors captured from the existing node implementation;
no C++ code is needed to build or run the Rust tests.

### Linux contract tests

After building the standalone target, run:

```sh
python3 test/test_mmxwallet_cli.py --binary build/mmxwallet -v
```

The tests use disposable keys, a localhost mock RPC, and Python's standard library. They cover machine output,
private stdin input, Unicode, custom key paths, operation without curl/PATH, exact amounts, memo boundaries, file protection/no overwrite,
RPC errors, transaction status, and preparation/broadcast of identical bytes. They do not contact the public
RPC or spend real funds. Windows and macOS are not yet qualified by this test suite.

## Node CLI

To check on the node: `mmx node info`

To check on the peers: `mmx node peers`

To check on a transaction: `mmx node tx <txid>`

To show current node height: `mmx node get height`

To dump a transaction: `mmx node get tx <txid>`

To dump a contract: `mmx node get contract <address>`

To get balance for an address: `mmx node get balance <address> -x <currency>`

To get raw balance for an address: `mmx node get amount <address> -x <currency>`

To dump a block: `mmx node get block <height>`

To dump a block header: `mmx node get header <height>`

To show connected peers: `mmx node get peers`

To show estimated netspace: `mmx node get netspace`

To show circulating coin supply: `mmx node get supply`

To call a smart contract const function: `mmx node call`

To show smart contract state variables: `mmx node read`

To dump all storage of a smart contract: `mmx node dump`

To dump assembly code of a smart contract: `mmx node dump_code`

To fetch a block from a peer: `mmx node fetch block <peer> <height>`

To fetch a block header from a peer: `mmx node fetch header <peer> <height>`

To check the balance of an address: `mmx node balance <address>`

To check the history of an address since a particlar block height: `mmx node history <address> <block_height>`

To csv export the history of an address since a particlar block height: `mmx node history_csv <address> <block_height>`

To show all offers: `mmx node offers [open | closed]`

To force a re-sync: `mmx node sync`

To replay/revert to an earlier block height: `mmx node revert <height>`

## Wallet CLI

To show everything in a wallet: `mmx wallet show`

To show wallet balances: `mmx wallet show balance`

To show wallet contracts: `mmx wallet show contracts`

To show wallet offers: `mmx wallet show offers`

To get a specific wallet address: `mmx wallet get address`

To get a specific wallet balance: `mmx wallet get balance`

To get a specific raw wallet balance: `mmx wallet get amount`

To get a list of all contract addresses: `mmx wallet get contracts`

To get the mnemonic seed words of a wallet: `mmx wallet get seed`

To show entire wallet activity : `mmx wallet log`

To csv export entire wallet activity : `mmx wallet log_csv`

To show recent wallet activity : `mmx wallet log -N <limit>`

To transfer funds: `mmx wallet send <options>`
```
  -a <amount to send, 1.23>
  -r <tx fee multiplier>
  -t <destination address>
  -x <currency>
```
To withdraw funds from a contract: `mmx wallet send_from -s <address>`

To transfer an NFT, same as sending with one satoshi: `mmx wallet transfer`

To create an offer on the chain: `mmx wallet offer`
```
  (-x / -z default MMX)
  -a <bid amount> -b <ask amount>
  -x <bid currency> -z <ask currency>
```
To accept an offer: `mmx wallet accept <address>`

To mint tokens: `mmx wallet mint`
```
  -a <amount>
  -t <destination>
  -x <currency>
```
To deploy a contract: `mmx wallet deploy <file>`

To execute a smart contract function: `mmx wallet exec <function> <args> -x <contract>`

To deposit funds to a smart contract: `mmx wallet deposit <function> <args>`
```
  -a <amount to send, 1.23>
  -t <contract address>
  -x <currency>
```

To create a new wallet (offline): `mmx wallet create -f [file_name] [--with-passphrase]`

To create a new wallet (online): `mmx wallet new [name] [--with-passphrase]`

To restore a wallet from a seed hash: `mmx wallet create --with-seed`

To restore a wallet from a set of 24 mnemonic words: `mmx wallet create --with-mnemonic`

To show all wallets and their index: `mmx wallet accounts`

To get farmer / pool keys for plotting: `mmx wallet keys`

To lock a wallet if passphase enabled: `mmx wallet lock`

To unlock a wallet with passphrase: `mmx wallet unlock`

**To use a non-default wallet, specify `-j <index>` in combination with the above commands. See `mmx wallet accounts`.**

## Farmer CLI

To check on the farm: `mmx farm info`

To get total space in bytes: `mmx farm get space`

To show plot directories: `mmx farm get dirs`

To add plot directories: `mmx farm add <dir>`

To remove plot directories: `mmx farm remove <dir>`

To reload plots: `mmx farm reload`

## Harvester CLI

To check on the harvester: `mmx harvester info`

To get harvester space in bytes: `mmx harvester get space`

To show plot directories: `mmx harvester get dirs`

To add plot directories: `mmx harvester add <dir>`

To remove plot directories: `mmx harvester remove <dir>`

To reload plots: `mmx harvester reload`

## Pooling

To use pooling first create a plot NFT, then create plots for it and finally join a pool.

### Create Plot NFT

```bash frame="none"
mmx wallet plotnft create <name>
```

`<name>` can be any string without whitespace.

After creation the plot NFT is in solo farming mode, which means block rewards will directly go to your Farmer reward address.
See below how to join a pool.

:::note[Note]
Need to wait for the transaction to confirm before it will show up.\
This command takes the usual `-j <index>` argument to select a different wallet.
:::

### Show Plot NFTs

```bash frame="none"
mmx wallet plotnft show
```

The address shown in `[...]` is the plot NFT contract address, which needs to be used for plotting.

:::note[Note]
This command takes the usual `-j <index>` argument to select a different wallet.
:::

Example:
```bash frame="none"
mmx wallet plotnft show -j 1
```
```
[mmx1wknv8xxvzjafrswsrwr3l85y6d8nms2dz22dgxl2qpcyjp64amtsjqjna5]
  Name: test1
  Locked: true
  Server URL: http://localhost:8080
```

### Join a Pool

First you need to obtain the pool server URL for the pool, via their website or discord.

```bash frame="none"
mmx wallet plotnft join <pool_url> -x <plot_nft_address>
```

`<plot_nft_address>` is the same as used for plotting, see `mmx wallet plotnft show`.

:::note[Note]
This command does not need any `-j` to select a wallet.
:::

Example:
```bash frame="none"
mmx wallet plotnft join http://localhost:8080 -x mmx1wknv8xxvzjafrswsrwr3l85y6d8nms2dz22dgxl2qpcyjp64amtsjqjna5
```

### Leave Pool

```bash frame="none"
mmx wallet plotnft unlock -x <plot_nft_address>
```

This will take 256 blocks to complete, to avoid cheating.
Once complete the plot NFT is in solo farming mode, which means block rewards will directly go to your Farmer reward address.

:::note[Note]
This command does not need any `-j` to select a wallet.
:::

Example:
```bash frame="none"
mmx wallet plotnft unlock -x mmx1wknv8xxvzjafrswsrwr3l85y6d8nms2dz22dgxl2qpcyjp64amtsjqjna5
```

### Switch Pool

First leave the current pool, wait 256 blocks for the plot NFT to unlock, then join the new pool as shown above.

### Show Info

To see pool account: `mmx pool info`

To see partials info: `mmx farm info`

Example:
```bash frame="none"
mmx pool info
```
```
Pool [http://localhost:8080]
  Balance: 1.5 MMX
  Total Paid: 123.456 MMX
  Difficulty: 1
  Pool Share: 33 %
  Partial Rate: 33.5417 per hour
  Blocks Found: 42
  Estimated Space: 13.3789 TB
```

```bash frame="none"
mmx farm info
```
```
Plot NFT [mmx1wknv8xxvzjafrswsrwr3l85y6d8nms2dz22dgxl2qpcyjp64amtsjqjna5]
  Name: test1
  Server URL: http://localhost:8080
  Target Address: mmx1uj2dth7r9tcn3vas42f0hzz74dkz8ygv59mpx44n7px7j7yhvv4sfmkf0d
  Plot Count: 100
  Points: 309 OK / 0 FAIL
  Difficulty: 1
  Avg. Response: 0.515676 sec
  Last Partial: 2024-10-13 23:25:44
```
