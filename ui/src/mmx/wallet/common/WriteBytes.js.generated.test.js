import { describe, it, assert, expect } from "vitest";

import { tx_note_e } from "./tx_note_e";
import { addr_t, bytes_t } from "./addr_t";
import { WriteBytes } from "./WriteBytes";

import "../utils/Uint8ArrayUtils";
import { Execute } from "./Operation";
import { optional } from "./optional";
import { Variant } from "./Variant";
import { txin_t, txout_t } from "./txio_t";
import { pair } from "./pair";
import { vnxObject } from "./vnxObject";

describe("WriteBuffer", () => {
    it("bool true", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", true);
        const jsHex = wb.buffer.toHex();
        const cppHex = "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D6501";
        assert.equal(jsHex, cppHex);
    });

    it("bool false", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", false);
        const jsHex = wb.buffer.toHex();
        const cppHex = "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D6500";
        assert.equal(jsHex, cppHex);
    });

    it("uint64_t", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", 1337n);
        const jsHex = wb.buffer.toHex();
        const cppHex = "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D653905000000000000";
        assert.equal(jsHex, cppHex);
    });

    it("uint64_t min", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", 0n);
        const jsHex = wb.buffer.toHex();
        const cppHex = "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D650000000000000000";
        assert.equal(jsHex, cppHex);
    });

    it("uint64_t max", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", 18446744073709551615n);
        const jsHex = wb.buffer.toHex();
        const cppHex = "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D65FFFFFFFFFFFFFFFF";
        assert.equal(jsHex, cppHex);
    });

    it("int64_t", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", 1337n);
        const jsHex = wb.buffer.toHex();
        const cppHex = "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D653905000000000000";
        assert.equal(jsHex, cppHex);
    });

    it("int64_t min", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", -9223372036854775808n);
        const jsHex = wb.buffer.toHex();
        const cppHex = "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D650000000000000080";
        assert.equal(jsHex, cppHex);
    });

    it("int64_t max", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", 9223372036854775807n);
        const jsHex = wb.buffer.toHex();
        const cppHex = "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D65FFFFFFFFFFFFFF7F";
        assert.equal(jsHex, cppHex);
    });

    it("string", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", "string");
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D65737472696E673C3E0600000000000000737472696E67";
        assert.equal(jsHex, cppHex);
    });

    it("bytes_t", () => {
        const wb = new WriteBytes();
        wb.write_field(
            "field_name",
            new bytes_t(new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]))
        );
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D6562797465733C3E1000000000000000000102030405060708090A0B0C0D0E0F";
        assert.equal(jsHex, cppHex);
    });

    it("bytes_t empty", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new bytes_t());
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D6562797465733C3E0000000000000000";
        assert.equal(jsHex, cppHex);
    });

    it("bytes_t addr_t", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new addr_t("mmx1ckyz0x7fpet4y7zmckyg7lklp8dc5gdr2kjd8hamk49rnk8zu9eq2cnz7a"));
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D6562797465733C3E200000000000000072E1E2D8394AB5BBDFD3A455A3218ADB09DF7E8F88C55B7852570EC99B2788C5";
        assert.equal(jsHex, cppHex);
    });

    it("vector", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]));
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D6562797465733C3E1000000000000000000102030405060708090A0B0C0D0E0F";
        assert.equal(jsHex, cppHex);
    });

    it("vector string", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", ["1337", "hello", "world"]);
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D65766563746F723C3E0300000000000000737472696E673C3E040000000000000031333337737472696E673C3E050000000000000068656C6C6F737472696E673C3E0500000000000000776F726C64";
        assert.equal(jsHex, cppHex);
    });

    it("vector uint64_t", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]);
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D65766563746F723C3E100000000000000000000000000000000100000000000000020000000000000003000000000000000400000000000000050000000000000006000000000000000700000000000000080000000000000009000000000000000A000000000000000B000000000000000C000000000000000D000000000000000E000000000000000F00000000000000";
        assert.equal(jsHex, cppHex);
    });

    it("vector empty", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new Uint8Array([]));
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D6562797465733C3E0000000000000000";
        assert.equal(jsHex, cppHex);
    });

    it("Variant empty", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new Variant());
        const jsHex = wb.buffer.toHex();
        const cppHex = "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D654E554C4C";
        assert.equal(jsHex, cppHex);
    });

    it("Variant bool true", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new Variant(true));
        const jsHex = wb.buffer.toHex();
        const cppHex = "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D6501";
        assert.equal(jsHex, cppHex);
    });

    it("Variant bool false", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new Variant(false));
        const jsHex = wb.buffer.toHex();
        const cppHex = "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D6500";
        assert.equal(jsHex, cppHex);
    });

    it("Variant int", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new Variant(255));
        const jsHex = wb.buffer.toHex();
        const cppHex = "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D65FF00000000000000";
        assert.equal(jsHex, cppHex);
    });

    it("Variant string", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new Variant("1337"));
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D65737472696E673C3E040000000000000031333337";
        assert.equal(jsHex, cppHex);
    });

    it("Variant vector", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new Variant([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]));
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D65766563746F723C3E100000000000000000000000000000000100000000000000020000000000000003000000000000000400000000000000050000000000000006000000000000000700000000000000080000000000000009000000000000000A000000000000000B000000000000000C000000000000000D000000000000000E000000000000000F00000000000000";
        assert.equal(jsHex, cppHex);
    });

    it("Variant vnx::Object", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new Variant(new vnxObject({ field1: "1337", field2: 1337 })));
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D656F626A6563743C3E766563746F723C3E0200000000000000706169723C3E737472696E673C3E06000000000000006669656C6431737472696E673C3E040000000000000031333337706169723C3E737472696E673C3E06000000000000006669656C64323905000000000000";
        assert.equal(jsHex, cppHex);
    });

    it("vnx::Object", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new vnxObject({ field1: "1337", field2: 1337 }));
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D656F626A6563743C3E766563746F723C3E0200000000000000706169723C3E737472696E673C3E06000000000000006669656C6431737472696E673C3E040000000000000031333337706169723C3E737472696E673C3E06000000000000006669656C64323905000000000000";
        assert.equal(jsHex, cppHex);
    });

    it("vnx::Object empty", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new vnxObject());
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D656F626A6563743C3E766563746F723C3E0000000000000000";
        assert.equal(jsHex, cppHex);
    });

    it("txout_t", () => {
        const wb = new WriteBytes();
        wb.write_field(
            "field_name",
            new txout_t({
                address: "mmx1ckyz0x7fpet4y7zmckyg7lklp8dc5gdr2kjd8hamk49rnk8zu9eq2cnz7a",
                contract: "mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev",
                amount: 255,
                memo: "memo",
            })
        );
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D6574786F75745F743C3E62797465733C3E200000000000000072E1E2D8394AB5BBDFD3A455A3218ADB09DF7E8F88C55B7852570EC99B2788C562797465733C3E20000000000000000000000000000000000000000000000000000000000000000000000000000000FF0000000000000000000000000000006F7074696F6E616C3C3E01737472696E673C3E04000000000000006D656D6F";
        assert.equal(jsHex, cppHex);
    });

    it("txin_t", () => {
        const wb = new WriteBytes();
        wb.write_field(
            "field_name",
            new txin_t({
                address: "mmx1ckyz0x7fpet4y7zmckyg7lklp8dc5gdr2kjd8hamk49rnk8zu9eq2cnz7a",
                contract: "mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev",
                amount: 255,
                memo: "memo",
                solution: 255,
                flags: 255,
            })
        );
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D657478696E5F743C3E62797465733C3E200000000000000072E1E2D8394AB5BBDFD3A455A3218ADB09DF7E8F88C55B7852570EC99B2788C562797465733C3E20000000000000000000000000000000000000000000000000000000000000000000000000000000FF0000000000000000000000000000006F7074696F6E616C3C3E01737472696E673C3E04000000000000006D656D6F";
        assert.equal(jsHex, cppHex);
    });

    it("optional", () => {
        const wb = new WriteBytes();
        wb.write_field(
            "field_name",
            new optional(new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]))
        );
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D656F7074696F6E616C3C3E0162797465733C3E1000000000000000000102030405060708090A0B0C0D0E0F";
        assert.equal(jsHex, cppHex);
    });

    it("optional nullptr", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new optional(null));
        const jsHex = wb.buffer.toHex();
        const cppHex = "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D656F7074696F6E616C3C3E00";
        assert.equal(jsHex, cppHex);
    });

    it("pair", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new pair("test1", 255));
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D65706169723C3E737472696E673C3E05000000000000007465737431FF00000000000000";
        assert.equal(jsHex, cppHex);
    });

    it("map", () => {
        const wb = new WriteBytes();
        wb.write_field(
            "field_name",
            new Map([
                ["test1", 255],
                ["test2", 255],
            ])
        );
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D65766563746F723C3E0200000000000000706169723C3E737472696E673C3E05000000000000007465737431FF00000000000000706169723C3E737472696E673C3E05000000000000007465737432FF00000000000000";
        assert.equal(jsHex, cppHex);
    });

    it("map empty", () => {
        const wb = new WriteBytes();
        wb.write_field("field_name", new Map([]));
        const jsHex = wb.buffer.toHex();
        const cppHex =
            "6669656C643C3E737472696E673C3E0A000000000000006669656C645F6E616D65766563746F723C3E0000000000000000";
        assert.equal(jsHex, cppHex);
    });
});
