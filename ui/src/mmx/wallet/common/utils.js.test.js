import { assert, describe, expect, it } from "vitest";
import { cost_to_fee, get_num_bytes } from "./utils";

describe("utils cost_to_fee", () => {
    it("#1", () => {
        assert.equal(cost_to_fee(0, 2048), 0n);
    });

    it("#2", () => {
        assert.equal(cost_to_fee(1, 2048), 2n);
    });

    it("#3", () => {
        const value = Number.MAX_SAFE_INTEGER;
        assert.equal(cost_to_fee(value, 2048), 2n * BigInt(value));
    });
});

describe("utils get_num_bytes", () => {
    it("invalid type", () => {
        expect(() => get_num_bytes(Symbol("invalid type"))).toThrowError();
    });
});
