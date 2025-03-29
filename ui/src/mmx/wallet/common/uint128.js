export class uint128 {
    #value = null;

    #lower = null;
    #upper = null;

    valueOf() {
        return this.#value;
    }

    lower() {
        return this.#lower;
    }

    upper() {
        return this.#upper;
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

        this.#value = _value;
        this.#lower = _value & 0xffffffffffffffffn;
        this.#upper = (_value >> 64n) & 0xffffffffffffffffn;
    }
}
