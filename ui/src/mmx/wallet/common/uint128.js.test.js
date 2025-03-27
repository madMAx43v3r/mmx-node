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

    it("invalid value", () => {
        expect(() => new uint128([])).toThrowError();
    });

    it("number", () => {
        const value = new uint128(0x1337133713371337);
        assert.equal(value.lower(), 0x1337133713371337);
        assert.equal(value.upper(), 0x0);
    });

    it("string", () => {
        const value = new uint128("0xffffffffffffffff1337133713371337");
        assert.equal(value.lower(), 0x1337133713371337n);
        assert.equal(value.upper(), 0xffffffffffffffffn);
    });
});
