# MMX Pooling protocol

## Notes
- MMX addresses are encoded via bech32 string
- Hashes and signatures are encoded via little-endian upper-case hex string, without `0x` prefix
- Account address = payout address

## Pool Server API

### POST /partial

Endpoint to post a partial.

Content-Type: `application/json`

Payload: Object
- `height`: Block height the proof is for
- `hash`: Message hash for signature (see below)
- `challenge`: VDF challenge (hash) for this height and fork
- `account`: Payout address (can be multiple per NFT, first partial creates account)
- `proof`: Proof, can be Proof of Space or Proof of Stake (see below)
- `pool_url`: The base URL used to make this request
- `harvester`: Harvester name, usually hostname
- `partial_diff`: Partial difficulty used when performing lookup (might be different than current setting)
- `lookup_time_ms`: Harvester lookup time (ms)
- `farmer_sig`: Farmer signature based on `hash` and `proof.farmer_key`
- `__type`: `mmx.Partial`

The `proof` is an object as follows:
- `__type`: Proof type (`mmx.ProofOfSpaceNFT` or `mmx.ProofOfStake`)
- `score`: Proof score
- `plot_id`: Plot ID
- `farmer_key`: Farmer Public Key

For `proof.__type` == `mmx.ProofOfSpaceNFT` the following additional fields will be present:
- `ksize`: Plot k-size
- `seed`: Plot random seed
- `proof_xs`: List of proof X values (256)
- `contract`: Plot NFT contract address

On success returns status 200 without payload.

On error returns status 400 with `application/json` object as follows:
- `error_code`: Integer error code (see below)
- `error_message`: Message string

### GET /difficulty

Returns current partial difficulty.
If farmer account does not exist yet, it will return a default starting difficulty.

Will be polled by farmers every 60 sec by default.

Query parameters:
- `account`: Farmer account address (payout address)

Should always return status 200 with `application/json` and as payload just the integer.

### POST /set_difficulty

Endpoint for custom partial difficulty settings. Support for this is optional.

Partial difficulty is set per account (ie. payout address), not per plot NFT.

Content-Type: `application/json`

Payload: Object
- `account`: Account address
- `type`: `static` or `dynamic`
- `value`: Static difficulty value, or partials per hour (integer) in case of dynamic
- `time`: POSIX timestamp in seconds
- `public_key`: Public key for account address
- `signature`: Signature for message and `public_key`

On success returns status 200 without payload.

On error returns status 400 with `application/json` object as follows:
- `error_code`: Integer error code (see below)
- `error_message`: Message string

### GET /pool_info

Returns pool information as `application/json` object:
- `name`
- `description`
- `fee`: Pool fee (0 to 1)
- `logo_url`
- `protocol_version`: `1`
- `unlock_delay`: Plot NFT unlock delay
- `pool_target`: Plot NFT target address
- `min_difficulty`: Lowest partial difficulty that is supported

## Node API

### POST /wapi/node/verify_partial

Endpoint to verify a partial, optionally with plot NFT verification.

Note:
Partials should be verfied after a certain delay, for example once the height of the partial has been reached on-chain.
Partials are computed 6 blocks in advance, so waiting until the partial's height has been reached will give 6 blocks security.
Even in case of a re-org larger than 6 blocks, the chance a partial won't be valid anymore is only ~5%.

Content-Type: `application/json`

Payload: Object
- `partial`: The same object as was received by the pool via POST `/partial`.
- `pool_target`: Expected plot NFT target address (optional)
	- When specified will verify plot NFT is locked to given target address at current blockchain height.

On success returns status 200 without payload.

On error returns status 400 with `application/json` object as follows:
- `error_code`: Integer error code (see below)
- `error_message`: Message string

When the node is not synced will return status 500 with error text.

### POST /wapi/node/verify_plot_nft_target

Endpoint to verify that a plot NFT is locked and pointed to the given target address at the current blockchain height.

Content-Type: `application/json`

Payload: Object
- `address`: Plot NFT address
- `pool_target`: Expected plot NFT target address

On success returns status 200 without payload.

On error returns status 400 with `application/json` object as follows:
- `error_code`: Integer error code (see below)
- `error_message`: Message string

When the node is not synced will return status 500 with error text.

## Message hash algorithm

A string is generated for JSON objects as follows:

- First the `__type` field is added, followed with `/`
- Then for each field in the specified order:
	- The field name is added, followed with a `:`
	- The field value is added (floating point values are floored first)
- For any nested objects, the same scheme is followed.

The final string is hashed via `SHA2-256`.







