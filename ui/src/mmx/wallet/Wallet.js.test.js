import { describe, it, assert } from "vitest";

import { ECDSA_Wallet } from "./ECDSA_Wallet";
import { addr_t } from "./common/addr_t";
import { Wallet } from "./Wallet";

const mnemonic = import.meta.env.VITE_TEST_MNEMONIC;
const ecdsaWallet = new ECDSA_Wallet(mnemonic, "");

describe("Wallet", () => {
    const currency = new addr_t().toString();
    const dst_addr = "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6";
    const amount = 1;
    const options = {
        memo: "test",
        expire_at: -1,
        network: "mainnet",
        dev: {
            nonce: 1n,
        },
    };

    it("getSendTxAsync", async () => {
        const tx = await Wallet.getSendTxAsync(ecdsaWallet, amount, dst_addr, currency, options);
        assert.equal(tx.id, "4EEC4FF00DE1ED2138384B9FD6B9116F86897BF4CADDDF2BA071231B9520EAE9");
    });
});
