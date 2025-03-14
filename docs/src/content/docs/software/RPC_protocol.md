---
title: RPC Protocol
description: MMX Node RPC Protocol.
---

Every node exposes a HTTP endpoint on `localhost:11380` (by default).
Binding to `0.0.0.0` needs to be enabled in `config/local/HttpServer.json`.

The native API can be accessed via `/api/`.
A web-friendly `/wapi/` is available for select features.

## Authentication

Authentication is required to access private API endpoints or remove limits from public enpoints.

The best way to setup static authentication is to create an API token, by editing `config/local/HttpServer.json`:
```
{
	"token_map": [
		["secure_random_token_here", "ADMIN"]
	]
}
```
A secure random token can be generated with the `generate_passwd` tool.
`ADMIN` allows access to every endpoint, even spending from the wallet.
`USER` does not allow wallet spending.

To use the token, a HTTP header `x-api-token: <token>` needs to be specified for every request.
The name of this header can be changed by setting `token_header_name` in `HttpServer.json`.

## Native API

The native API can be accessed via `/api/`:
- `/api/node/` to access Node API
- `/api/router/` to access Router API
- `/api/wallet/` to access Wallet API
- `/api/farmer/` to access Farmer API
- `/api/harvester/` to access Harvester API

Documentation for those are found in [modules](https://github.com/madMAx43v3r/mmx-node/tree/master/modules).

`GET` can be used for `const` methods.
`POST` is needed for `non-const` methods, with a JSON object payload containing the function arguments.
Results are returned in JSON format.

Hashes and addresses are returned as byte arrays, not as a hex or bech32 string.
However they can be specified this way for arguments.

## WebAPI (WAPI)

A web friendly API is available to make developing web applications easier.

In case of an error a plaintext error message is returned, with a status code != 200.

### Public WAPI

The public section of the WAPI can be accessed without authentication, but limits are enforced to prevent large queries.
This is what RPC servers provide.

#### GET /wapi/chain/info

Returns an object with global parameters, see [ChainParams.vni](https://github.com/madMAx43v3r/mmx-node/blob/master/interface/ChainParams.vni).

#### GET /wapi/node/info

Returns an object with current network info, see [NetworkInfo.vni](https://github.com/madMAx43v3r/mmx-node/blob/master/interface/NetworkInfo.vni).

#### GET /wapi/header

Query parameters:
- `hash`: Block hash, or:
- `height`: Block height

Returns block header object, or status 404 if not found.

#### GET /wapi/headers

Query parameters:
- `limit`: Result limit (default = 20)
- `offset`: Optional starting height

Returns array of most recent block headers.

#### GET /wapi/block

Query parameters:
- `hash`: Block hash, or:
- `height`: Block height

Returns full block object, or status 404 if not found.

#### GET /wapi/transaction

Query parameters:
- `id`: Transaction ID (hex string)

Returns transaction info object (see [tx_info_t.vni](https://github.com/madMAx43v3r/mmx-node/blob/master/interface/tx_info_t.vni)), or status 404 if not found.

#### GET /wapi/transactions

Returns array of most recent transaction info objects, or list of transaction info objects for a given block height. 

Query parameters:
- `limit`: Result limit (default = 100)
- `height`: Optional block height
- `offset`: Optional starting index when `height` is specified

#### GET /wapi/address

Query parameters:
- `id`: Address (bech32)

Returns object:
- `balances`: Array of balance objects
- `contract`: Contract object if not a regular wallet address

#### GET /wapi/address/history

Query parameters:
- `id`: Address (bech32)
- `limit`: Result limit (default = 100)
- `since`: Since block height, inclusive (default = 0)
- `until`: Until block height, inclusive (default = current height)
- `type`: Optional type filter: `RECEIVE`, `SPEND`, `TXFEE`, `REWARD`, `VDF_REWARD`
- `currency`: Optional curreny filter (bech32 token address)

Returns an array of history objects, same as `/wapi/wallet/history`.

#### GET /wapi/balance

Query parameters:
- `id`: Address (bech32)
- `limit`: Optional result limit (default = 100)
- `currency`: Optional currency address (bech32)

Returns object:
- `balances`: Array of balance objects
- `nfts`: Array of NFT addresses

#### GET /wapi/contract

Query parameters:
- `id`: Address (bech32)

Returns contract object, or `null` in case not a contract.

#### GET /wapi/plotnft

Query parameters:
- `id`: Address (bech32)

Returns PlotNFT info, or status 404 if not a PlotNFT.

#### GET /wapi/contract/exec_history

Query parameters:
- `id`: Address (bech32)
- `limit`: Result limit (default = 100)

Returns an array of most recent contract execution objects (see [exec_entry_t.vni](https://github.com/madMAx43v3r/mmx-node/blob/master/interface/exec_entry_t.vni)).

#### GET /wapi/contract/storage

Query parameters:
- `id`: Address (bech32)

Returns object with contract storage fields.

#### GET /wapi/contract/storage/field

Query parameters:
- `id`: Address (bech32)
- `name`: Field name

Returns value of specified contract storage field.

#### GET /wapi/contract/storage/entry

Query parameters:
- `id`: Address (bech32)
- `name`: Field name, or:
- `addr`: Field address
- `key`: Map key

Returns value for a specified key in a storage map.

#### GET /wapi/farmers

Query parameters:
- `limit`: Result limit (default = 100)

Returns an array of objects in descending order:
- `farmer_key`: Farmer public key
- `block_count`: Number of blocks farmed since genesis

#### GET /wapi/farmer

Query parameters:
- `id`: Farmer public key (hex string)
- `since`: Optional starting height (default = 0)

Returns farmer summary object.

#### GET /wapi/farmer/blocks

Query parameters:
- `id`: Farmer public key (hex string)
- `limit`: Result limit (default = 50)
- `since`: Optional starting height (default = 0)

Returns array of recently farmed block headers.

#### POST /wapi/transaction/validate

Validates a given transaction as specified via request payload and returns execution result object (see [exec_result_t.vni](https://github.com/madMAx43v3r/mmx-node/blob/master/interface/exec_result_t.vni)):
- `did_fail`: Boolean, if transaction failed (but still able to include)
- `error`: Error object (see [exec_error_t.vni](https://github.com/madMAx43v3r/mmx-node/blob/master/interface/exec_error_t.vni)) in case of failure, otherwise `null`
- `total_cost`: Total transaction cost
- `total_fee`: Total fee as raw integer (`total_cost` times fee ratio)
- `total_fee_value`: Total fee in MMX (with decimals)

Returns status 400 with error message in case static validation failed, for example due to:
- Invalid hash
- Invalid signature
- Insufficient balance for TX fee

Note: Clients must handle `nonce` field as a 64-bit integer, truncation will cause static failure.

#### POST /wapi/transaction/broadcast

Validates and sends a given transaction as specified via request payload.
Will skip sending and return status 400 if static validation failed.
Returns status 200 on success.

### Wallet WAPI

Common parameters:
- `index`: Wallet index, see `/wapi/wallet/accounts` or `mmx wallet accounts`.
- `options`: Object with additional options:
	- `auto_send`: If to send transaction automatically (default = true)
	- `mark_spent`: If to mark transaction as spent in cache (default = false)
	- `fee_ratio`: TX fee ratio as integer (default = 1024)
	- `gas_limit`: Limit for extra dynamic cost (default = 5 MMX)
	- `expire_at`: Optional expiration height
	- `expire_delta`: Expiration delta, relative to current height (default = 100, `null` to disable)
	- `nonce`: Custom nonce (needs be > 0)
	- `user`: Custom user for contract calls (default = first address)
	- `sender`: Custom sender address (who will pay TX fee, default = first address)
	- `passphrase`: Optional passphrase to atomically unlock wallet.
	- `note`: Transaction note, see [tx_note_e.vni](https://github.com/madMAx43v3r/mmx-node/blob/master/interface/tx_note_e.vni).
	- `memo`: Optional memo string (max. 64 chars)

Endpoints that send a transaction will first validate it, and if invalid sending is aborted (saving TX fees).

#### GET /wapi/wallet/accounts

Returns an array of account objects:
- `account`: Wallet index
- `address`: First wallet address
- `name`: Custom wallet name
- `index`: Sub-account index, zero by default.
- etc

#### GET /wapi/wallet/account

Query parameters:
- `index`: Wallet index

Returns specific wallet info, same as `wallet/accounts`.

#### GET /wapi/wallet/balance

Query parameters:
- `index`: Wallet index
- `currency`: Optional currency address (bech32)
- `with_zero`: If to include zero balances (default = false)

Returns an object:
- `balances`: Array of balance objects
- `nfts`: Array of NFT addresses

#### GET /wapi/wallet/address

Query parameters:
- `index`: Wallet index
- `limit`: Result limit (default = 1)
- `offset`: Starting offset (default = 0)

Returns an array of wallet addresses in case limit > 1, otherwise returns single address at specified offset.

#### GET /wapi/wallet/tokens

Returns an array of white-listed token objects.

#### GET /wapi/wallet/history

Query parameters:
- `index`: Wallet index
- `limit`: Result limit (default = 100)
- `since`: Since block height, inclusive (default = 0)
- `until`: Until block height, inclusive (default = current height)
- `type`: Optional type filter: `RECEIVE`, `SPEND`, `TXFEE`, `REWARD`, `VDF_REWARD`
- `memo`: Optional memo filter (exact matches only)
- `currency`: Optional curreny filter (bech32 token address)

Returns an array of objects:
- `height`: Block height (4294967295 in case pending)
- `txid`: Transaction ID or block hash in case of `REWARD` and `VDF_REWARD`
- `amount`: Token amount (integer)
- `value` Token value (float with decimals)
- `decimals`: Number of decimals
- etc

Note: This endpoint includes pending transactions, `is_pending` or `height` needs to be checked.

#### GET /wapi/wallet/tx_history

Query parameters:
- `index`: Wallet index
- `limit`: Result limit (default = 100)

Returns most recent sent transactions.

#### GET /wapi/wallet/keys

Query parameters:
- `index`: Wallet index

Returns an object:
- `farmer_public_key`: Farmer public key

#### GET /wapi/wallet/seed

Query parameters:
- `index`: Wallet index

Returns an object with the mnemonic seed:
- `words`: Array of words
- `string`: String of words with space in-between

#### POST /wapi/wallet/send

Send tokens to a single destination.

Request payload is an object of arguments:
- `index`: Wallet index
- `amount`: Floating point amount, either as float or string.
- `currency`: Token address (bech32, default = MMX)
- `dst_addr`: Destination address (bech32)
- `src_addr`: Optional source address (bech32)
- `raw_mode`: Boolean, if true `amount` is parsed as a raw integer (default = false)
- `options`: Object with options

Returns transaction object if successful.

#### POST /wapi/wallet/send_many

Send tokens to many destinations in a single transaction.

Same as `/wapi/wallet/send`, except:
- `amounts`: Array of pairs: `[[address, amount], ...]`
- `src_addr`: Not supported

#### POST /wapi/wallet/send_off

Send a given transaction, while marking it as spent in the cache.

Request payload is an object of arguments:
- `index`: Wallet index
- `tx`: Transaction object

Returns status 200 if successful.

Note: Clients must handle `nonce` field as a 64-bit integer, truncation will cause static failure.

#### POST /wapi/wallet/deploy

Deploy a contract.

Request payload is an object of arguments:
- `index`: Wallet index
- `payload`: Contract object
- `options`: Object with options

Returns contract address (bech32) if successful.

Example `payload` for a smart contract:
```
{
	"__type": "mmx.contract.Executable",
	"name": "Fake Testnet USD",
	"symbol": "USDM",
	"decimals": 4,
	"binary": "mmx17pzl9cgmesyjur7rvvev70fx7f55rvyv7549cvrtf7chjml4qh4sryu9yl",
	"init_args": ["mmx1kx69pm743rshqac5lgcstlr8nq4t93hzm8gumkkxmp5y9fglnkes6ve09z"]
}
```

#### POST /wapi/wallet/execute

Execute a single smart contract function.

Request payload is an object of arguments:
- `index`: Wallet index
- `address`: Contract address
- `method`: Method name
- `args`: Array of function arguments (positional)
- `user`: Optional custom user (default = first address)
- `options`: Object with options

Returns transaction object if successful.












