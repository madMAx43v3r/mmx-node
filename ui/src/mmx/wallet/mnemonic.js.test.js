import { assert, describe, expect, it } from "vitest";
import { mnemonicToSeed, seedToWords, randomWords } from "./mnemonic";

import "./utils/Uint8ArrayUtils";
import { hash_t } from "./common/addr_t";
import { hexToBytes } from "@noble/hashes/utils";

describe("mnemonic #1", async () => {
    const mnemonic =
        "apple apple apple apple apple apple apple apple apple apple apple apple apple apple apple apple apple apple apple apple apple apple apple anchor";
    const hashStr = "0AA1542A8550AA1542A8550AA1542A8550AA1542A8550AA1542A8550AA1542A8";

    it("mnemonicToSeed", () => {
        const seed = mnemonicToSeed(mnemonic);
        assert.equal(seed.toReversed().toHex(), hashStr);
    });

    it("seedToWords", () => {
        const seed = hexToBytes(hashStr);
        const words = seedToWords(seed);
        assert.equal(words, mnemonic);
    });
});

describe("mnemonic #2", async () => {
    const mnemonic =
        "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon art";
    const hashStr = new hash_t().toString();

    it("mnemonicToSeed", () => {
        const seed = mnemonicToSeed(mnemonic);
        assert.equal(seed.toReversed().toHex(), hashStr);
    });

    it("seedToWords", () => {
        const seed = hexToBytes(hashStr);
        const words = seedToWords(seed);
        assert.equal(words, mnemonic);
    });
});

describe("mnemonic #3", async () => {
    it("randomWords", () => {
        const mnemonic = randomWords();
        assert.isNotEmpty(mnemonic);
    });
});
