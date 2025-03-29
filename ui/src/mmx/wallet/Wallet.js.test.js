import { describe, it, assert } from "vitest";

import { ECDSA_Wallet } from "./ECDSA_Wallet";
import { addr_t } from "./common/addr_t";
import { Wallet } from "./Wallet";

import "./Transaction.ext";
import { JSONbigNative } from "./utils/JSONbigNative";

const mnemonic = import.meta.env.VITE_TEST_MNEMONIC;
const ecdsaWallet = new ECDSA_Wallet(mnemonic, "");

import { txs } from "./Transaction.js.txs.test.js";

describe("Wallet", () => {
    const currency = new addr_t().toString();
    const amount = 1;
    const options = {
        memo: "test",
        expire_at: -1,
        network: "mainnet",
        nonce: 1n,
    };

    it("getSendTxAsync", async () => {
        const dst_addr = "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6";
        const tx = await Wallet.getSendTxAsync(ecdsaWallet, amount, dst_addr, currency, options);

        assert.equal(tx.id, "4EEC4FF00DE1ED2138384B9FD6B9116F86897BF4CADDDF2BA071231B9520EAE9");
        assert.equal(tx.content_hash, "B6FF6E13CFF6844A001ED888E60A0042F9EF0CB06C5F8B74F6AD207622BED70F");

        assert.equal(tx.aux.feeAmount, "60000");
        assert.equal(tx.aux.feeValue, 0.06);
    });

    it("getSendManyTxAsync", async () => {
        const tx = await Wallet.getSendManyTxAsync(
            ecdsaWallet,
            [
                { address: "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6", amount: 1 },
                { address: "mmx1mw38rg8jcy2tjc5r7sxque6z45qrw6dsu6g2wmhahwf30342rraqyhsnea", amount: 1 },
            ],
            currency,
            options
        );

        assert.equal(tx.id, "CB60F7A2B26FB3FEFD26F78514C7AD5C5B22BCD50FD785EDFB09937AAC41C8DB");
        assert.equal(tx.content_hash, "2801E075F89F81D83172EB1157B7DDB576DA5AC72C53A27B9687F86C34803204");

        assert.equal(tx.aux.feeAmount, "90000");
        assert.equal(tx.aux.feeValue, 0.09);
    });

    it("getExecuteTxAsync", async () => {
        const txTest = txs.get("OFFER UPDATE");
        const json = JSONbigNative.stringify(JSONbigNative.parse(txTest.json));
        const hex = txTest.hex;

        const _options = {
            ...options,
            expire_at: 591282,
            nonce: 17669814118440932621n,
        };

        const address = "mmx1lr5vtm5sx5yspj6283hv3zqrp0jc450yzxdt4adv62v7gqty6gsqxn3nk5";
        const method = "set_price";
        const args = ["0x010000000000000000"];
        const user = 0;

        const tx = await Wallet.getExecuteTxAsync(ecdsaWallet, address, method, args, user, _options);

        assert.equal(tx.toString(), json);

        const hash_serialize = tx.hash_serialize(true);
        assert.equal(hash_serialize.toHex(), hex);
    });
});
