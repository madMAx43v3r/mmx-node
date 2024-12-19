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
- `contract`: Plot NFT address
- `account`: Payout address (can be multiple per NFT, first partial creates account)
- `pool_url`: The base URL used to make this request
- `harvester`: Harvester name, usually hostname
- `difficulty`: Partial difficulty used when performing lookup (might be different than current setting)
- `lookup_time_ms`: Harvester lookup time (ms)
- `proof`: Proof, can be Proof of Space or Proof of Stake (see below)
- `farmer_sig`: Farmer signature based on `hash` and `proof.farmer_key`
- `__type`: `mmx.Partial`

The `proof` is an object as follows:
- `__type`: Proof type (always `mmx.ProofOfSpaceNFT`)
- `score`: Proof score
- `plot_id`: Plot ID
- `challenge`: challenge hash for this height and fork
- `farmer_key`: Farmer Public Key

For `proof.__type` == `mmx.ProofOfSpaceNFT` the following additional fields will be present:
- `ksize`: Plot k-size
- `seed`: Plot random seed
- `proof_xs`: List of proof X values (256)
- `contract`: Plot NFT contract address

Returns status 200 with `application/json` object as follows:
- `error_code`: Integer error code (see below)
- `error_message`: Message string

#### Example partial
TODO: update
```
{
  __type: 'mmx.Partial',
  account: 'mmx1kx69pm743rshqac5lgcstlr8nq4t93hzm8gumkkxmp5y9fglnkes6ve09z',
  challenge: 'CA98DB6504B2C85D876BEC1881F41E805D49FDE33DFE548A26C3277CFAE7EFF9',
  contract: 'mmx19hxc7mfkdalx0khe5nr4r50t338z8sg4avn7vqu7ts4mpkxeg0lqhesvzl',
  farmer_sig: '3CDCD57B151254835100F47F16834F91ED87838F5C68E113F236FD88169133643D06A4D50B6FB96859BECB73A73CE0E449262717B9907E70F05659F487C14F4B',
  harvester: 'prime',
  hash: 'B7269935091572E1004B48C327EEF2070DA0176A94AE2129AD8E68A9059FD95F',
  height: 845490,
  lookup_time_ms: 82,
  difficulty: 1,
  pool_url: 'http://localhost:8080',
  proof: {
    __type: 'mmx.ProofOfSpaceNFT',
    contract: 'mmx19hxc7mfkdalx0khe5nr4r50t338z8sg4avn7vqu7ts4mpkxeg0lqhesvzl',
    farmer_key: '027D7562FB5A8967E57A22F876302F75BA3AE3980607FB32E4439681F820AE398F',
    ksize: 26,
    plot_id: '22F0CA184633E07F32C5289B0ACF39597637CD6685D3CD4E7CD87CD56FAF8761',
    proof_xs: [
      61796699, 13346431, 24374059, 22439072, 65129000, 58802027,
      ... 250 more items
    ],
    score: 25993,
    seed: '091B11F9A066A9120AAD4BF8D30D933E4CED2EB3BF2CCA1DA72D8BA8E68BDA96'
  }
}
```

### GET /difficulty

Returns current partial difficulty.
If farmer account does not exist yet, it should return a default starting difficulty.

Will be polled by farmers every 300 sec by default.

Query parameters:
- `id`: Farmer account address (payout address)

Always returns status 200 with `application/json` object as follows:
- `difficulty`: Integer value

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

Returns status 200 with `application/json` object as follows:
- `error_code`: Integer error code (see below)
- `error_message`: Message string

### GET /pool/info

Returns pool information as `application/json` object:
- `name`
- `description`
- `fee`: Pool fee (0 to 1)
- `logo_path`: Relative path for logo image, starting with `/`.
- `protocol_version`: `1`
- `pool_target`: Plot NFT target address
- `min_difficulty`: Lowest partial difficulty that is supported

### GET /pool/stats

Returns pool statistics as `application/json` object:
- `estimated_space`: Estimated pool size in TB
- `partial_rate`: Partials / hour
- `farmers`: Number of active farmers

### GET /account/info

Query parameters:
- `id`: Farmer account address (payout address)

Returns account info as `application/json` object:
- `balance`: Current unpaid balance [MMX]
- `total_paid`: Total amount paid out so far [MMX]
- `difficulty`: Partial difficulty
- `pool_share`: Relative farm size to pool (0 to 1)
- `partial_rate`: Partials / hour
- `blocks_found`: Number of blocks farmed
- `estimated_space`: Estimated farm size in TB

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

Returns status 200 with `application/json` object as follows:
- `error_code`: Integer error code (see below)
- `error_message`: Message string

When the node is not synced will return status 500 with error text.

### POST /wapi/node/verify_plot_nft_target

Endpoint to verify that a plot NFT is locked and pointed to the given target address at the current blockchain height.

Content-Type: `application/json`

Payload: Object
- `address`: Plot NFT address
- `pool_target`: Expected plot NFT target address

Returns status 200 with `application/json` object as follows:
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

## Error Codes
Error codes are transferred as strings in JSON:
- `NONE`: No error
- `INVALID_PARTIAL`
- `DUPLICATE_PARTIAL`
- `PARTIAL_TOO_LATE`
- `PARTIAL_NOT_GOOD_ENOUGH`
- `CHALLENGE_REVERTED`
- `CHALLENGE_NOT_FOUND`
- `INVALID_PROOF`
- `INVALID_DIFFICULTY`
- `INVALID_SIGNATURE`
- `INVALID_CONTRACT`
- `INVALID_AUTH_KEY`
- `INVALID_ACCOUNT`
- `INVALID_TIMESTAMP`
- `POOL_LOST_SYNC`
- `SERVER_ERROR`





