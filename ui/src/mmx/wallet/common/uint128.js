export class uint128 {
    #value = 0n;

    constructor(value) {
        if (typeof value === "string") {
            this.#value = BigInt(value);
        } else if (typeof value === "number") {
            this.#value = BigInt(value);
        } else if (typeof value === "bigint") {
            this.#value = value;
        } else {
            throw new Error("Invalid uint128 value");
        }
    }

    lower() {
        return this.#value & 0xffffffffffffffffn;
    }

    upper() {
        return (this.#value >> 64n) & 0xffffffffffffffffn;
    }
}
