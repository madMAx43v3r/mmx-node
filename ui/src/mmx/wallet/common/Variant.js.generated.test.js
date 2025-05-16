import { describe, it, assert, expect } from "vitest";

import { Variant } from "./Variant";
import { get_num_bytes } from "./utils";

import "../utils/Uint8ArrayUtils";
describe("Variant", () => {
    // generated from C++
    it("null", () => {
        const variant = new Variant(null);
        const jsHex = variant.data.toHex();
        const cppHex = "01000000";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 1;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("bool true", () => {
        const variant = new Variant(true);
        const jsHex = variant.data.toHex();
        const cppHex = "01001F0001";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 1;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("bool false", () => {
        const variant = new Variant(false);
        const jsHex = variant.data.toHex();
        const cppHex = "01001F0000";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 1;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("zero", () => {
        const variant = new Variant(0);
        const jsHex = variant.data.toHex();
        const cppHex = "0100010000";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 8;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("unsigned integer 8bit", () => {
        const variant = new Variant(255);
        const jsHex = variant.data.toHex();
        const cppHex = "01000100FF";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 8;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("unsigned integer 16bit", () => {
        const variant = new Variant(65535);
        const jsHex = variant.data.toHex();
        const cppHex = "01000200FFFF";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 8;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("unsigned integer 32bit", () => {
        const variant = new Variant(4294967295);
        const jsHex = variant.data.toHex();
        const cppHex = "01000300FFFFFFFF";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 8;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("unsigned integer 64bit", () => {
        const variant = new Variant(18446744073709551615n);
        const jsHex = variant.data.toHex();
        const cppHex = "01000400FFFFFFFFFFFFFFFF";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 8;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("signed integer 8bit", () => {
        const variant = new Variant(-128);
        const jsHex = variant.data.toHex();
        const cppHex = "0100050080";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 8;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("signed integer 16bit", () => {
        const variant = new Variant(-32768);
        const jsHex = variant.data.toHex();
        const cppHex = "010006000080";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 8;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("signed integer 32bit", () => {
        const variant = new Variant(-2147483648);
        const jsHex = variant.data.toHex();
        const cppHex = "0100070000000080";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 8;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("signed integer 64bit", () => {
        const variant = new Variant(-9223372036854775808n);
        const jsHex = variant.data.toHex();
        const cppHex = "010008000000000000000080";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 8;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("string", () => {
        const variant = new Variant("1337");
        const jsHex = variant.data.toHex();
        const cppHex = "010020000400000031333337";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 8;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("string empty", () => {
        const variant = new Variant("");
        const jsHex = variant.data.toHex();
        const cppHex = "0100200000000000";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 4;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("list", () => {
        const variant = new Variant([1, 2, 3, 255]);
        const jsHex = variant.data.toHex();
        const cppHex = "02000C0011000400000001000100010100010002010001000301000100FF";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 36;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("object empty", () => {
        const variant = new Variant({});
        const jsHex = variant.data.toHex();
        const cppHex = "0100180000000000";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 4;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("object", () => {
        const variant = new Variant({ bool: true, int: 1337, string: "1337" });
        const jsHex = variant.data.toHex();
        const cppHex =
            "010018000300000004000000626F6F6C01001F000103000000696E7401000200390506000000737472696E67010020000400000031333337";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 34;
        assert.equal(jsNumBytes, cppNumBytes);
    });

    it("object nested", () => {
        const variant = new Variant({
            bool: true,
            int: 1337,
            obj: { bool: true, int: 1337, string: "1337" },
            string: "1337",
        });
        const jsHex = variant.data.toHex();
        const cppHex =
            "010018000400000004000000626F6F6C01001F000103000000696E74010002003905030000006F626A010018000300000004000000626F6F6C01001F000103000000696E7401000200390506000000737472696E6701002000040000003133333706000000737472696E67010020000400000031333337";
        assert.equal(jsHex, cppHex);

        const jsNumBytes = get_num_bytes(variant);
        const cppNumBytes = 71;
        assert.equal(jsNumBytes, cppNumBytes);
    });
});
