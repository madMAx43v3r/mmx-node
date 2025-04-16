import { assert, describe, expect, it } from "vitest";
import { spend_options_t } from "./spend_options_t";

describe("spend_options_t", () => {
    it("no prams", () => {
        expect(() => new spend_options_t()).toThrowError();
    });

    it("no network", () => {
        const params = {
            expire_at: -1,
        };
        expect(() => new spend_options_t(params)).toThrowError();
    });

    it("no expire_at", () => {
        const params = {
            network: "mainnet",
        };
        expect(() => new spend_options_t(params)).toThrowError();
    });

    it("fee_ratio #1", () => {
        const params = {
            expire_at: -1,
            network: "mainnet",
        };
        const options = new spend_options_t(params);
        assert.equal(options.fee_ratio, 1024);
    });

    it("fee_ratio #2", () => {
        const params = {
            expire_at: -1,
            network: "mainnet",
            fee_ratio: 2048,
        };
        const options = new spend_options_t(params);
        assert.equal(options.fee_ratio, 2048);
    });
});
