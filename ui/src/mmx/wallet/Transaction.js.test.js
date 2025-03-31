import { describe, it, assert } from "vitest";

import { Transaction } from "./Transaction";
import { getChainParamsAsync } from "./utils/getChainParamsAsync";

import "./Transaction.ext";

import { JSONbigNative } from "./utils/JSONbigNative";
import "./utils/Uint8ArrayUtils";

import { txs } from "./Transaction.js.txs.test.js";

txs.forEach((item, key) => {
    describe(`Transaction #${key}`, () => {
        const json = item.json;
        const hex = item.hex;

        const _tx = Transaction.parse(json);
        const id = _tx.id;
        const content_hash = _tx.content_hash;

        it("parse", () => {
            const tx = Transaction.parse(json);
            assert.equal(tx.toString(), JSONbigNative.stringify(JSONbigNative.parse(json)));
        });

        it("calc_hash id", () => {
            const tx = Transaction.parse(json);
            const hash = tx.calc_hash(false);
            assert.equal(hash.toHex(), id);
        });

        it("calc_hash content_hash", () => {
            const tx = Transaction.parse(json);

            const hash_serialize = tx.hash_serialize(item.full_hash ?? true);
            const hash = tx.calc_hash(true);

            assert.equal(hash_serialize.toHex(), hex);
            assert.equal(hash.toHex(), content_hash);
        });

        it("finalize", () => {
            const tx = Transaction.parse(json);
            tx.id = null;
            tx.finalize();
            assert.equal(tx.id, id);
        });

        it("static_cost", async () => {
            const tx = Transaction.parse(json);
            const chainParams = await getChainParamsAsync(tx.network);
            const static_cost = tx.calc_cost(chainParams);
            assert.equal(static_cost, tx.static_cost);
        });
    });
});
