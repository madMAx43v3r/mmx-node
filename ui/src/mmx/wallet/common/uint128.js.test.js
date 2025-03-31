import { describe, it, assert, expect } from "vitest";
import { uint128 } from "./uint128";

describe("uint128", () => {
    it("lower", () => {
        const value = new uint128(0xffffffffffffffff1337133713371337n);
        assert.equal(value.lower(), 0x1337133713371337n);
        assert.equal(value.upper(), 0xffffffffffffffffn);
    });

    it("upper", () => {
        const value = new uint128(0x1337133713371337ffffffffffffffffn);
        assert.equal(value.lower(), 0xffffffffffffffffn);
        assert.equal(value.upper(), 0x1337133713371337n);
    });

    it("negative", () => {
        expect(() => new uint128(-1n)).toThrowError();
    });

    it("overflow", () => {
        expect(() => new uint128(0xffffffffffffffffffffffffffffffffn + 1n)).toThrowError();
    });

    it("unsupported type", () => {
        expect(() => new uint128(Symbol("unsupported type"))).toThrowError();
    });

    it("number", () => {
        const value = new uint128(0x1337133713371337);
        assert.equal(value.valueOf(), 0x1337133713371337);
        assert.equal(value.lower(), 0x1337133713371337);
        assert.equal(value.upper(), 0x0);
    });

    it("string", () => {
        const value = new uint128("0xffffffffffffffff1337133713371337");
        assert.equal(value.valueOf(), 0xffffffffffffffff1337133713371337n);
        assert.equal(value.lower(), 0x1337133713371337n);
        assert.equal(value.upper(), 0xffffffffffffffffn);
    });

    it("toHex", () => {
        assert.equal(new uint128(100000).toHex(), "0x186a0");
        assert.equal(new uint128(100).toHex(), "0x64");

        const inv_price = (100000n << 64n) / 100n;
        assert.equal(new uint128(inv_price).toHex(), "0x3e80000000000000000");
    });
});
