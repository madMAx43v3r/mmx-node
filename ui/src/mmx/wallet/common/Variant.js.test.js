import { describe, it, assert, expect } from "vitest";

import "../utils/Uint8ArrayUtils";
import { Variant } from "./Variant";

describe("Variant", () => {
    it("unsupported type", () => {
        expect(() => new Variant(Symbol("unsupported type"))).toThrowError();
    });

    it("deep recursion", () => {
        let obj = 1337;
        for (let index = 1; index <= 100 - 1; index++) {
            obj = { index, obj };
        }
        expect(() => new Variant(obj)).not.toThrowError();
        expect(() => new Variant({ obj: obj })).toThrowError();
    });

    it("size", () => {
        const variant = new Variant({
            bool: true,
            int: 1337,
            obj: { bool: true, int: 1337, string: "1337" },
            string: "1337",
        });
        assert.equal(variant.size(), 119);
    });
});
