# MMX Script

The MMX smart contract language is a restricted subset of JavaScript with some additional features.

## Types

- Null (`null`)
- Boolean (`true` / `false`)
- Integer (256-bit unsigned)
- Binary (aka. "binary string", same as `ArrayBuffer` in JS)
- String (UTF-8 encoded)
- Array
- Map

Note: Objects are maps with string keys.

## Deviations from JavaScript

- `var` has local scope (behaves like `let`)
- Integer overflows / underflows will fail execution (except for `unsafe_*(...)`)
- Reading un-initialized variables will fail execution (instead of returning `undefined`)
- Reading non-existent map values will return `null` (instead of `undefined`)
- `==` comparison is strict (ie. same as `===` in JS)
- `+` only supports integer addition (need to use `concat()` for strings)
- `$` is not supported in variable / function names
- `delete` is a function (not an operator) and only works on variables directly

## Not supported from JavaScript

- Classes (KISS)
- Signed integers / negative values
- Floating point values / arithmetic
- Function variables (all functions are global like in `C`)
- `let` (because `var` is already like `let`)
- `undefined` (you'll get `null` or execution failure instead)
- `for(... of ...)` loops over maps or objects (supported only for arrays)
- `for(... in ...)` style loops
- `===` operator (because `==` is strict already)
- `**`, `?.`, `??` operators
- `(condition ? ifTrue : ifFalse)`
- `switch()`
- `function*`
- `void`, `new`, `class`, `typeof`, `await`, `async`, `with`, `super`
- `import`, `export`, `throw`, `try`, `catch`, `instanceof`, `yield`
- Template strings
- Multiple assignment: `[a, b] = arr, {a, b} = obj`
- Spread syntax `(...)`
- Any built-in classes like:
	- `Array`, `Map`, `Set`, `Time`, `Date`, `Number`, `Math`
	- `Error`, `Object`, `Function`, `Boolean`, `Symbol`
	- `String`, `RegExp`, `Promise`, `Iterator`, `Proxy`
- Any built-in functions like:
	- `eval()`, `escape()`, `unescape()`

## Additional features

- `const` function modifier
	- `function get_price() const {}`
	- `const` functions cannot modify contract state
- `public` function modifier
	- `function payout() public {}`
	- Only public functions can be executed via transactions.
- `payable` function modifier
	- `function trade(...) public payable {}`
	- Required to support deposits with function call.
	- A `deposit()` function is always `payable`.

- Special `this` object to access built-in variables:
	- `txid`: The transaction ID (Type: 32-bytes or `null`)
	- `height`: The block height at which the code is executed (Type: 256-bit unsigned int)
	- `balance`: Map of contract balances (Type: Map[32-bytes] = 256-bit unsigned int)
	- `address`: Contract address (Type: 32-bytes)
	- `user`: A user address can be specified when executing a contract function,
		which is verified via a signature before executution, same as `msg.sender` in EVM.
		(Type: 32-bytes or `null`)
	- `deposit`: Object to check for deposited currency and amount (for `payable` functions):
		- `currency`: Currency address (Type: 32-bytes)
		- `amount`: Amount deposited (Type: 256-bit unsinged int)

## Operators (sorted by rank)

- `.`: field access (objects / maps only)
- `++`: pre or post increment (integers only)
- `--`: pre or post decrement (integers only)
- `!`: Logical NOT (boolean only)
- `~`: Bitwise NOT (integer only)
- `*`: Multiplication (integers only)
- `/`: Division (integers only)
- `%`: Modulo division (integers only)
- `+`: Addition (integers only, see `concat()` for strings)
- `-`: Subtraction (integers only)
- `>>`: Right shift (integers only)
- `<<`: Left shift (integers only)
- `<`: Less than (integers only)
- `>`: Greater than (integers only)
- `<=`: Less than or equal (integers only)
- `>=`: Greater than or equal (integers only)
- `!=`: Not equal (any types)
- `==`: Equals (any types, strict, no implicit conversions)
- `&`: Bitwise AND (integers only)
- `&&`: Logical AND (boolean only)
- `^`: Bitwise XOR (integers only)
- `|`: Bitwise OR (integers only)
- `||`: Logical OR (boolean only)
- `=`: Right to left assignment
- `+=`: Inplace addition (integers only)
- `-=`: Inplace subtraction (integers only)
- `*=`: Inplace multiplication (integers only)
- `/=`: Inplace division (integers only)
- `>>=`: Inplace right-shift (integers only)
- `<<=`: Inplace left-shift (integers only)

## Built-in functions

- `size(array)`: Returns an array's size
- `push(array, value)`: Pushes a value onto the end of an array
- `pop(array)`: Pops a value off the back of an array
- `set(map, key, value):`: Same as `map[key] = value` or `array[key] = value` (with integer key)
- `get(map, key)`: Same as `map[key]` or `array[key]` (with integer key)
- `erase(map, key)`: Same as `map[key] = null`
- `delete(v)`: Deletes a variable (read after delete will fail, re-assign is possible)
- `min(a, b)`: Returns smaller value (integers only)
- `max(a, b)`: Returns larger value (integers only)
- `clone(v)`: Makes a (deep) copy and returns reference
- `deref(v)`: Returns a (deep) copy (without reference) of the value given by a reference
- `typeof(v)`: Returns an integer to denote a variable's type:
	- 0 = `null`
	- 1 = `false`
	- 2 = `true`
	- 4 = Integer (256-bit unsinged)
	- 5 = String
	- 6 = Binary
	- 7 = Array
	- 8 = Map
- `concat(a, b)`: Returns concatenation of two (binary) strings (like `a + b` in JS)
- `memcpy(src, count, [offset])`
	- Returns a sub-string of `src` with length `count` starting at `offset`
	- `offset` defaults to `0`
	- `src` must be a string or binary string
	- Out of bounds access will fail execution
- `fail(message, [code])`: Fails execution with string `message` and optional integer `code`
- `bech32(addr)`: Parses a bech32 address string and returns 32 bytes.
	- Returns 32 zero bytes if no argument given, which corresponds to the zero address.
- `binary(v)`: Converts to a binary
	- Returns binary for strings (1-to-1 copy)
	- Returns little endian 32-byte binary for integers
- `binary_le(v)`: Same as `binary()` except:
	- Returns big endian 32-byte binary for integers
- `binary_hex(v)`: Same as `binary()` except:
	- Parses input string as a hex string, with optional `0x` prefix.
- `bool(v)`: Converts to boolean
	- Returns `false` for: `null`, `false`, `0`, empty (binary) string
	- Otherwise returns `true`
- `uint(v)`: Converts to 256-bit unsigned integer
	- `null` => 0, `false` => 0, `true` => 1
	- `"123"` => 123, `"0b10"` => 2, `"0xFf"` => 255
	- Binary strings are parsed in big endian: `[00, FF]` => `0x00FF` / `255`
- `uint_le(v)`: Same as `uint()` except binary strings are parsed in little endian.
- `uint_hex(v)`: Same as `uint()` except strings are parsed in hex, even without `0x` prefix.
- `to_string(v)`: Converts to a string
	- Integers are converted to decimal
	- String inputs are returned as-is
	- Binary strings are converted as-is (like memcpy())
- `to_string_hex(v)`: Same as `to_string()` except:
	- Converts integers and binary strings to a hex string, without `0x` prefix.
- `to_string_bech32(v)`: Same as `to_string()` except:
	- Converts binary string to a bech32 address string `mmx1...` (fails if not 32 bytes)
	- Converts `null` to zero address string `mmx1qqqq...`
- `send(address, amount, [currency], [memo])`: Transfer funds from contract to an address
	- `address` is destination address as 32-byte binary
	- `amount` is integer amount, fails if larger than 64-bit
	- `currency` is currency address as 32-byte binary, defaults to MMX (ie. `bech32()`)
	- `memo` is an optional memo string (fails if longer than 64 chars)
	- Does nothing if `amount` is zero
	- Fails if balance is insufficient
	- Returns `null` (ie. nothing)
- `mint(address, amount, [memo])`: Mint new tokens of contract and send to address
	- `address` is destination address as 32-byte binary
	- `amount` is integer amount, fails if larger than 64-bit
	- `memo` is an optional memo string (fails if longer than 64 chars)
	- Does nothing if `amount` is zero
	- Returns `null` (ie. nothing)
	- This is the only way to mint tokens on MMX blockchain
- `sha256(msg)`: Computes SHA-2 256-bit hash for given input message (binary string)
	- Returns hash as 32 bytes
- `ecdsa_verify(msg, pubkey, signature)`: Verifies a ECDSA signature
	- Returns `true` if valid, otherwise `false`
	- `msg` must be 32 bytes
	- `pubkey` must be 33 bytes
	- `signature` must be 64 bytes
- `rcall(contract, method, [arg0], [arg1], ...)`: Calls function of another contract and returns value
	- `contract` is a string idendifier of the contract to call (specified at deploy time)
	- `method` is the method name as string (without `()`)
	- Returns value from the function call
- `read(field, [address])`:
	- Returns the value of a non-storage contract field (which is constant)
	- `address` is current contract if not specified
	- Storage fields cannot be read with this function, need to use `rcall()` instead
- `log(level, message)`: For debugging / testing only
	- Logs a string message at integer `level`
- `event(name, data)`: For debugging / testing only
	- Logs an event with string `name` and arbitrary `data`
- `__nop()`: Injects an `OP_NOP` instruction (for debugging)

## Fixed-point Arithmetic

Because floating point values and arithmetic are not supported, one has to use fixed-point arithmetic.

First you need to choose how much precision is needed.
A good choice would be 64-bits or 96-bits, such that a 256-bit overflow is not possible when adding a lot of 128-bit values.
Account balances can be up to 128-bit, while amounts (for deposit and sending) are limited to 64-bit. 

Let's say we choose 64-bit precision.
This means we multiply all constants by 2^64 (or left shift by 64) and right shift by 64 to get a result.
For example `123456 * 1.337 = 165060.672`:
```
const factor = 24663296826549670511; 		// 1.337
const result = (123456 * factor) >> 64;		// 165060
```
Fixed point arithmetic always rounds down.
When converting floating point constants, rounding to nearest is best for accuracy.

## Passing function arguments from the outside

Function argument passing from the outside is limited to what JSON can describe.
This means no integer values greater than 64-bit and no binary strings.

In order to pass an address, pass it as a bech32 encoded string (`mmx1..`) and then decode via `bech32(...)`.

In order to pass binary strings, encode it as a hex string (`0x...`) and then decode via `binary_hex(...)`.

In order to pass integers greater than 64-bits you can encode them as a decimal or hex string (`0x...`)
and then decode via `uint(...)`. Hex encoding is recommended, since decimal decoding is expensive.

## Examples

### Contract Storage

Any global variable will be persisted accross the lifetime of a contract:
```
var storage = [];
function add(value) public {
	push(storage, value);
}
function get() const public {
	return storage;
}
```

### Constructor

Any private function can be a constructor. When deploying a contract you have to specify which function to use. \
The default is to use `init()`:
```
var foo;
function init(bar) {
	foo = bar;
}
```

### Deposit

Deposits in MMX are made through function calls to a `payable` function:
```
var currency = bech32();
var balances = {};
function deposit(account) public payable {
	if(this.deposit.currency != currency) {
		fail("invalid currency");
	}
	balances[account] += this.deposit.amount;
}
```
Note: If a function is called "deposit" the `payable` modifier can be omitted.
Note: `this.balance` already includes the deposited amount, it always equals the amount that can be spent via `send()`.

Trying to deposit funds via a non-`payable` function is not possible.
However it's possible to send funds to a contract via normal transfer.
In this case no function is called, and the contract needs to handle this case implicitly.
It cannot be avoided since it's possible to send to a contract's address before deployment.

Funds that are sent to a contract (via normal transfer) at height `H` will only be visible to the contract at height `H+1`.

Any funds not spent in the transaction that deploys a contract will be credited to the contract's address before the contructor is called.
This allows a more efficient way to deploy with funding, compared to executing a deposit function.

### Built-in Contracts

- [offer.js](../src/contract/offer.js) - Offer
	- Allows to trade between two currencies at a fixed price (See GUI -> Market)
	- Takers can trade any fraction of the offer
	- Maker can cancel / refill at any time
	- Bids are accumulated in the contract (for lower tx fees)
	- Manual withdrawal will transfer accumulated bids to maker wallet
- [swap.js](../src/contract/swap.js) - Swap
	- Liquidity pool AMM, similar to UniSwap (see GUI -> Swap)
	- Has 4 different fee-tiers, each with their own liquidty and price
	- A trade is divided into multiple chunks / iterations
	- `trade()` loops over all pools in multiple iterations and picks the best pool to trade with for each chunk (while taking the fee into account)
	- Supports one-sided liquidity
	- Liquidity is locked for 24 hours after it's been added / or fee-tier was changed
	- A single account can only provide liquidity for one fee-tier
	- Fee payouts are heuristic for better trade efficiency (manual trigger, no automatic compounding)
- [virtual_plot.js](../src/contract/virtual_plot.js) - Virtual Plot
	- Used for Proof Of Stake farming
	- Only 90% of deposited balance is returned on withdrawal, to avoid short-term staking.
- [plot_nft.js](../src/contract/plot_nft.js) - Plot NFT
	- Used for pooled farming to control rewards / switch pools
- [token.js](../src/contract/token.js) - Simple Token
	- Token contract with single owner to mint a token (without limits)
- [nft.js](../src/contract/nft.js) - NFT
	- Contract used to mint NFTs
	- Ensures only a single token is ever minted by a verified creator
- [time_lock.js](../src/contract/time_lock.js) - Simple Time Lock
- [escrow.js](../src/contract/escrow.js) - Simple Escrow with middle-man

### Minting tokens

Minting tokens is only possible via calling `mint()`, which mints new tokens of the contract. \
Every contract is also a currency, contract address = currency address.

A smart contract inherits from `mmx.contract.TokenBase`, which has the following fields:
- string `name`
- string `symbol`
- int `decimals` = 0
- vnx.Variant `meta_data`

If `decimals` is needed inside the code, it can be retrieved via `read("decimals")`, but usually this is not needed.
The same goes for `symbol`, etc.

### Deploying a Smart Contract

A smart contract's code is actually a separate "contract" of type `mmx.contract.Binary` which needs to be deployed first.

Multiple contracts can share the same binary, this reduces the cost of deploying contracts significantly.

If the code is not deployed on-chain yet, it needs to be compiled and deployed first:
```
mmx_compile -t -n testnet12 -f example.js -o example.dat
mmx wallet deploy example.dat
```
`mmx_compile` returns the binary address that we need to sepecify when deploying a contract later.
`-n testnet12` can be omitted for mainnet.

Once the binary is confirmed on-chain, we can deploy any number of contracts with the same code. \
This can be done via JSON files:
```
{
	"__type": "mmx.contract.Executable",
	"name": "Example",
	"symbol": "EXMPL",
	"decimals": 6,
	"binary": "mmx1...",
	"init_method": "init",
	"init_args": [...]
}
```
If the contract does not represent a token, `name`, `symbol` and `decimals` can be omitted.
If `init_method` is "init", it can be omitted as well since it's the default.

Now deploying a contract from JSON file:
```
mmx wallet deploy example.json
```
Every contract will have a different address since the wallet generates a random 64-bit transaction nonce by default.

The node keeps track which `sender` deployed a contract, as such you can view all your deployed contracts via:
```
mmx wallet show contracts
```

### Executing a smart contract function

Executing a smart contract function can be done via command line:
```
mmx wallet exec <method> [args] -x <contract>
```
This will submit a transaction to the network.

To make a deposit:
```
mmx wallet deposit <method> [args] -a <amount> -x <currency> -t <contract>
```
Be careful not to confuse `-x` and `-t`: `-t` is the destination, while `-x` is the currency to deposit.

By default `this.user` is set to the wallet's first address (index `0`).
This can be overriden with a different address index:
```
mmx wallet exec <method> [args] -k <index>
mmx wallet exec <method> [args] -k -1  # this will set `user` to `null`
```
The same applies to `mmx wallet deposit`.

Specific examples:
```
mmx wallet exec test_me 1 2 3 -x mmx1...
mmx wallet deposit test_me 1 2 3 -x mmx1... -t mmx1...  # `-t` is like `-x` for `exec`
mmx wallet deposit -x mmx1... -t mmx1...  # this will call `deposit()` without args
```

To execute a function just for testing:
```
mmx node call <method> [args] -x <contract>
```
This will not submit any transaction, but rather just simulate a call at the current blockchain `height + 1`.

### Inspecting a Contract

To view the contract that was deployed:
```
mmx node get contract <address>
```

To view a contract's state (global storage variables):
```
mmx node read -x <address>
mmx node read <field> -x <address>  # `field` can be a variable name or hex address `0x...`
```

To dump a contracts's entire storage:
```
mmx node dump -x <address>
```
`<0x...>` denotes a reference, `[0x...,size]` denotes an array, `{0x...}` denotes a map / object.

To dump a contract's binary code:
```
mmx node dump_code -x <address>
```

#### Address Ranges

Memory is unified in MMX, but there are regions for different usage:
- `0x0000000` to `0x3FFFFFF`: constant data
- `0x4000000` to `0x7FFFFFF`: external data (`this.*`)
- `0x8000000` to `0x47FFFFFF`: stack (`0x8000000` = return value, `0x8000001` = first argument)
- `0x48000000` to `0xFFFFFFFF`: global variables (stored in DB)
- `0x100000000` to `0xFFFFFFFFFFFFFFFF`: heap / dynamic storage (stored in DB, if not garbage collected)

## Special notes

### References

As in JavaScript: Arrays, Maps and Objects are reference counted.

That means "copying" by assignment does not make a deep copy, it only copies the reference:
```
const array = [1, 2, 3];
const tmp = array;
push(tmp, 4);
return array;	// returns [1, 2, 3, 4]
```
```
const object = {"foo": {}};
const foo = object.foo;
foo.bar = true;
return object;	// returns {foo: {"bar": true}}
```
In order to make a deep-copy you need to use `clone()`.

### clone() from storage

It's not possible to clone maps / objects from contract storage:
``` 
var object = {};
function test() public {
	return clone(object);		// will fail
}
```

### Account balances

Balances and amounts are always stored / specified in "sats" aka. "binks", meaning no decimals.
Floating point representation with decimals is always just for display purposes.

Contract code can only access the balances of itself,
not other contracts or addresses (since that would break parallel execution).
However it's possible to call a method of another contract that returns its balance.
In this case execution is serialized.

Note: `this.balance` is updated automatically when receiving funds via deposit, or when spending via `send()`.


