import { describe, it, assert, expect } from "vitest";

import * as secp256k1 from "@noble/secp256k1";
const { randomBytes } = secp256k1.etc;

import "./Uint8ArrayUtils";

describe("Uint8ArrayUtils", () => {
    it("incorrect length", () => {
        const t0 = new Uint8Array(0);
        expect(() => t0.first).toThrowError();
        expect(() => t0.second).toThrowError();

        const t63 = new Uint8Array(63);
        expect(() => t63.first).toThrowError();
        expect(() => t63.second).toThrowError();

        const t65 = new Uint8Array(65);
        expect(() => t65.first).toThrowError();
        expect(() => t65.second).toThrowError();
    });

    it("first/second", () => {
        const p1 = randomBytes(32);
        const p2 = randomBytes(32);
        const t = new Uint8Array([...p1, ...p2]);
        expect(t.first).toEqual(p1);
        expect(t.second).toEqual(p2);
    });
});
