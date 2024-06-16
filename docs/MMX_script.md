# MMX Script

The MMX smart contract language is a restricted subset of JavaScript with some additional features.

## Types

- Null (`null`)
- Boolean (`true` / `false`)
- 256-bit unsigned integer
- Binary (same as `ArrayBuffer` in JS)
- String (UTF-8 encoded)
- Array
- Map

Note: Objects are maps with string keys.

## Deviations from JavaScript

- `var` has local scope (same as `const`)
- Integer overflows / underflows will fail execution (except for `unsafe_*(...)`)
- Reading un-initialized variables will fail execution
- Reading non-existent map values will return `null` (instead of `undefined`)
- `==` is strict (ie. same as `===` in JS)
- `$` is not supported
- `delete` is a function (not an operator) and it only works on variables (no object fields)

## Not supported

- `undefined`
- Signed integers / negative values
- Floating point values / arithmetic
- Function variables / lambdas (all functions are global like in `C`)
- `for()` loops over maps or objects
- `===` operator (because `==` is already strict)

## Subset of JavaScript

The parts of JavaScript which are supported are as follows:

- `var` and `const` variables
- `function name(arg1, arg2)` functions (global scope only)
- `array[index]` to access an array element (will fail if out of bounds)
- `map[key]` to access a map value
- `object.field` to access an object field (or map value with string key)
- `if() else if() else()`
- `for(var i = 0; i < 10; i++)` style loops
- `for(const item of array)` style loops (for arrays only)
- JSON syntax to create arrays and objects

## Additional features

- `const` modifier for functions (`function get_price() const {}`)
	Const functions cannot modify contract state.
- `public` modifier for functions (`function payout() public {}`)
	Only public functions can be executed via transactions.
- `payable` modifier for functions (`function trade(...) public payable {}`)
	Required to support deposits with function call.

- Special `this` object to access built-in variables:
	- `txid`: The transaction ID (Type: 32-bytes or `null`)
	- `height`: The block height at which the code is executed (Type: 256-bit unsigned int)
	- `balance`: Map of contract balances (Type: Map[32-bytes] = 256-bit unsigned int)
	- `address`: Contract address (Type: 32-bytes)
	- `user`: A user address can be specified when executing a contract function,
		which is verified via a signature before executution. (Type: 32-bytes or `null`)
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
- `==`: Equals (any types, strict, no implict conversions)
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

In order to pass binary strings you can encode it as a hex string (`0x...`) and then decode via `binary_hex(...)`.

In order to pass integers greater than 64-bits you can encode them as a decimal or hex string (`0x...`)
and then decode via `uint(...)`. Hex encoding is recommended, since decimal decoding is expensive.

When creating a transaction in C++ code it is possible to pass binary strings and 256-bit integers.
However it's discouraged to rely on this when developing a contract, as it won't allow to deploy contracts from a JSON file.

## Special notes

### References

As in JavaScript: Arrays, Maps and Objects are reference counted.

This means "copying" by assignment does not make a deep copy, it only copies the reference:
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
In order to make a deep-copy you need to use the `clone()` function.

### clone() from storage

It's not possible to clone maps / objects from contract storage:
``` 
var object = {};
function test() public {
	return clone(object);		// will fail
}
```












