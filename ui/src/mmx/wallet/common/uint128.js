export class uint128 {
    #value = null;

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
    }

    lower() {
        return this.#value & 0xffffffffffffffffn;
    }

    upper() {
        return (this.#value >> 64n) & 0xffffffffffffffffn;
    }
}
