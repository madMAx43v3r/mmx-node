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

The message hash is computed

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

## Message hash algorithm

A string is generated for JSON objects as follows:

- First the `__type` field is added, followed with `/`
- Then for each field in the specified order:
	- The field name is added, followed with a `:`
	- The field value is added









