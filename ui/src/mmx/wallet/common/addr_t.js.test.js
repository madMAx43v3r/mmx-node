import { describe, it, assert, expect } from "vitest";
import { hexToBytes } from "@noble/hashes/utils";
import { addr_t } from "./addr_t";

import "../utils/Uint8ArrayUtils";

describe("addr_t", () => {
    const addrStr = "mmx1ckyz0x7fpet4y7zmckyg7lklp8dc5gdr2kjd8hamk49rnk8zu9eq2cnz7a";
    const addrHex = "72E1E2D8394AB5BBDFD3A455A3218ADB09DF7E8F88C55B7852570EC99B2788C5";

    it("str to bytes", () => {
        const addr = new addr_t(addrStr);
        assert.equal(addr.toString(), addrStr);
        assert.equal(addr.data().toHex(), addrHex);
    });

    it("bytes to str", () => {
        const addr = new addr_t(hexToBytes(addrHex));
        assert.equal(addr.toString(), addrStr);
        assert.equal(addr.data().toHex(), addrHex);
    });

    it("empty", () => {
        const addrEmptyHex = "0000000000000000000000000000000000000000000000000000000000000000";
        const addrStrEmpty = "mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev";

        const addr = new addr_t();
        assert.equal(addr.toString(), addrStrEmpty);
        assert.equal(addr.data().toHex(), addrEmptyHex);
    });

    it("invalid address prefix", () => {
        const invalidAddr = "xch1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq2u30kz";
        expect(() => new addr_t(invalidAddr)).toThrowError();
    });

    it("invalid address type", () => {
        expect(() => new addr_t(0)).toThrowError();
    });

    it("invalid length", () => {
        expect(() => new addr_t(new Uint8Array(0))).toThrowError();
        expect(() => new addr_t(new Uint8Array(31))).toThrowError();
    });
});
