import { describe, it, assert } from "vitest";

import { ECDSA_Wallet } from "./ECDSA_Wallet";
import { addr_t } from "./common/addr_t";
import { Wallet } from "./Wallet";

import "./Transaction.ext";
import { JSONbigNative } from "./utils/JSONbigNative";

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
        const json =
            '{"__type": "mmx.Transaction", "id": "1D0FB351DB849EAF624AE5CB6E5E7752CC1797F4F7EF00604D98EB857B9E23B2", "version": 0, "expires": 591282, "fee_ratio": 1024, "static_cost": 43300, "max_fee_amount": 5033300, "note": "EXECUTE", "nonce": 17669814118440932621, "network": "mainnet", "sender": "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6", "inputs": [], "outputs": [], "execute": [{"__type": "mmx.operation.Execute", "version": 0, "address": "mmx1lr5vtm5sx5yspj6283hv3zqrp0jc450yzxdt4adv62v7gqty6gsqxn3nk5", "solution": 0, "method": "set_price", "args": ["0x010000000000000000"], "user": "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6"}], "solutions": [{"__type": "mmx.solution.PubKey", "version": 0, "pubkey": "0344EE96D1B85CAC0F99B7CFA44F39EFFC590BDF51D45099D1F24AA09E5F9AD6E0", "signature": "43700FC7C7FF8AF154C6C46B6F8D2097613E0BC11D5CA6A024DBA563D51006072C06DF710BCD80B0952C3DBEFC3607CB8E998EF50C133B66EC35F2FBC485E10E"}], "deploy": null, "exec_result": null, "content_hash": "1A9F69335234CB16544432D24C31D0EF794456E4C2460EE4A8F7DFBC626F2404"}';

        const address = "mmx1lr5vtm5sx5yspj6283hv3zqrp0jc450yzxdt4adv62v7gqty6gsqxn3nk5";
        const method = "set_price";
        const args = ["0x010000000000000000"];
        const user = 0;

        const _options = {
            ...options,
            expire_at: 591282,
            nonce: 17669814118440932621n,
        };

        const tx = await Wallet.getExecuteTxAsync(ecdsaWallet, address, method, args, user, _options);
        assert.equal(tx.toString(), JSONbigNative.stringify(JSONbigNative.parse(json)));
    });
});
