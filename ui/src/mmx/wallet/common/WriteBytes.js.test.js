import { describe, it, assert, expect } from "vitest";

import { tx_note_e } from "./tx_note_e";
import { addr_t, bytes_t } from "./addr_t";
import { WriteBytes } from "./WriteBytes";

import "../utils/Uint8ArrayUtils";
import { Execute } from "./Operation";
import { optional } from "./optional";
import { Variant } from "./Variant";
import { txin_t, txout_t } from "./txio_t";

describe("WriteBuffer", () => {
    it("field without value", () => {
        const wb = new WriteBytes();
        wb.write_field("field");
        assert.equal(wb.buffer.toHex(), "6669656C643C3E737472696E673C3E05000000000000006669656C64");
    });

    it("unknown type", () => {
        const wb = new WriteBytes();
        expect(() => wb.write_field("field", Symbol("unknown object type"))).toThrowError();
    });

    it("unknown object type", () => {
        const wb = new WriteBytes();
        expect(() => wb.write_field("field", {})).toThrowError();
    });

    it("invalid number", () => {
        const wb = new WriteBytes();
        expect(() => wb.write_field("field", -0x7fffffff - 2)).toThrowError();
    });

    it("invalid number [float]", () => {
        const wb = new WriteBytes();
        expect(() => wb.write_field("field", -1337.1337)).toThrowError();
    });
});
