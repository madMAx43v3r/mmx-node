import { describe, it, assert } from "vitest";

import { ECDSA_Wallet } from "./ECDSA_Wallet";
import { addr_t } from "./common/addr_t";
import { Wallet } from "./Wallet";

import "./Transaction.ext";

const mnemonic = import.meta.env.VITE_TEST_MNEMONIC;
const ecdsaWallet = new ECDSA_Wallet(mnemonic, "");

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
        assert.equal(tx.aux.feeAmount, "90000");
        assert.equal(tx.aux.feeValue, 0.09);
    });
});
