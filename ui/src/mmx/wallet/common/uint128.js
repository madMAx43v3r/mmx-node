export class uint128 {
    static BITMASK_64 = 0xffffffffffffffffn;

    #lower = null;
    #upper = null;

    valueOf() {
        return (this.#upper << 64n) | this.#lower;
    }

    lower() {
        return this.#lower;
    }

    upper() {
        return this.#upper;
    }

    toHex() {
        return "0x" + this.valueOf().toString(16);
    }

    constructor(value) {
        let _value;
        if (typeof value === "string") {
            _value = BigInt(value);
        } else if (typeof value === "number") {
            _value = BigInt(value);
        } else if (typeof value === "bigint") {
            _value = value;
        } else {
            throw new Error("Unsupported type");
        }

        if (_value >> 128n) {
            throw new Error("uint128() overflow");
        }

        this.#lower = _value & uint128.BITMASK_64;
        this.#upper = (_value >> 64n) & uint128.BITMASK_64;
    }
}
