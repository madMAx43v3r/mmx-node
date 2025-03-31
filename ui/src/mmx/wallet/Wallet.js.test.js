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

    it("getSendTxAsync", async () => {
        const options = {
            memo: "test",
            expire_at: -1,
            network: "mainnet",
            nonce: 1n,
        };

        const amount = 1;
        const dst_addr = "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6";
        const tx = await Wallet.getSendTxAsync(ecdsaWallet, amount, dst_addr, currency, options);

        assert.equal(tx.id, "4EEC4FF00DE1ED2138384B9FD6B9116F86897BF4CADDDF2BA071231B9520EAE9");
        assert.equal(tx.content_hash, "B6FF6E13CFF6844A001ED888E60A0042F9EF0CB06C5F8B74F6AD207622BED70F");

        assert.equal(tx.aux.feeAmount, "60000");
        assert.equal(tx.aux.feeValue, 0.06);
    });

    it("getSendManyTxAsync", async () => {
        const options = {
            memo: "test",
            expire_at: -1,
            network: "mainnet",
            nonce: 1n,
        };

        const payments = [
            { address: "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6", amount: 1 },
            { address: "mmx1mw38rg8jcy2tjc5r7sxque6z45qrw6dsu6g2wmhahwf30342rraqyhsnea", amount: 1 },
        ];

        const tx = await Wallet.getSendManyTxAsync(ecdsaWallet, payments, currency, options);

        assert.equal(tx.id, "7390DDF31C3B1DE3B1F48B79E2FC1EFF69E113BA5C75B86CD82B80F1D06DB9CD");
        assert.equal(tx.content_hash, "03A9292DB568B15D5BFBEEEBB87103C5B0C9DF0CADA0D6F7A2B58746C428B934");

        assert.equal(tx.aux.feeAmount, "75000");
        assert.equal(tx.aux.feeValue, 0.075);
    });

    it("getExecuteTxAsync", async () => {
        const txTest = txs.get("OFFER UPDATE");
        const json = JSONbigNative.stringify(JSONbigNative.parse(txTest.json));
        const hex = txTest.hex;

        const options = {
            network: "mainnet",
            expire_at: 591282,
            nonce: 17669814118440932621n,
        };

        const address = "mmx1lr5vtm5sx5yspj6283hv3zqrp0jc450yzxdt4adv62v7gqty6gsqxn3nk5";
        const method = "set_price";
        const args = ["0x010000000000000000"];
        const user = 0;

        const tx = await Wallet.getExecuteTxAsync(ecdsaWallet, address, method, args, user, options);

        assert.equal(tx.toString(), json);

        const hash_serialize = tx.hash_serialize(true);
        assert.equal(hash_serialize.toHex(), hex);
    });

    it("getDepositTxAsync", async () => {
        const txTest = txs.get("SWAP ADD LIQUIDITY");
        const json = JSONbigNative.parse(txTest.json);
        const jsonTxt = JSONbigNative.stringify(json);
        const hex = txTest.hex;

        const options = {
            network: "mainnet",
            expire_at: json.expires,
            nonce: json.nonce,
            user: "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6",
        };

        const address = "mmx1rda9sdn9ypgcs07us2surft0rjtz0pkccdlmxpv0yftzs9zathfqeh5maw";
        const method = "add_liquid";
        const args = [1, 0];
        const amount = "1000000";

        const tx = await Wallet.getDepositTxAsync(ecdsaWallet, address, method, args, amount, currency, options);

        assert.equal(tx.toString(), jsonTxt);

        const hash_serialize = tx.hash_serialize(true);
        assert.equal(hash_serialize.toHex(), hex);
    });

    it("getDeployTxAsync", async () => {
        const txTest = txs.get("TOKEN DEPLOY");
        const json = JSONbigNative.parse(txTest.json);
        const jsonTxt = JSONbigNative.stringify(json);
        const hex = txTest.hex;

        const options = {
            network: "mainnet",
            expire_at: json.expires,
            nonce: json.nonce,
            user: "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6",
        };

        const contract = {
            name: "Test coin",
            symbol: "TEST",
            decimals: 6,
            meta_data: null,
            binary: "mmx17pzl9cgmesyjur7rvvev70fx7f55rvyv7549cvrtf7chjml4qh4sryu9yl",
            init_method: "init",
            init_args: ["mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6"],
            depends: [],
        };

        const tx = await Wallet.getDeployTxAsync(ecdsaWallet, contract, options);

        assert.equal(tx.toString(), jsonTxt);

        const hash_serialize = tx.hash_serialize(true);
        assert.equal(hash_serialize.toHex(), hex);
    });

    it("getMakeOfferTxAsync", async () => {
        const txTest = txs.get("OFFER DEPLOY");
        const json = JSONbigNative.parse(txTest.json);
        const jsonTxt = JSONbigNative.stringify(json);
        const hex = txTest.hex;

        const options = {
            network: "mainnet",
            expire_at: json.expires,
            nonce: json.nonce,
            user: "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6",
        };

        const bid_amount = 100000;
        const bid_currency = "mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev";
        const ask_amount = 100;
        const ask_currency = "mmx1ey6mxts9rcarq9jgl89su8cgex527plw3wskpnwxr4xgzkw4z65qxpj3fg";

        const tx = await Wallet.getMakeOfferTxAsync(
            ecdsaWallet,
            bid_amount,
            bid_currency,
            ask_amount,
            ask_currency,
            options
        );

        assert.equal(tx.toString(), jsonTxt);

        const hash_serialize = tx.hash_serialize(true);
        assert.equal(hash_serialize.toHex(), hex);
    });

    it("getOfferTradeTxAsync", async () => {
        const txTest = txs.get("OFFER TRADE");
        const json = JSONbigNative.parse(txTest.json);
        const jsonTxt = JSONbigNative.stringify(json);
        const hex = txTest.hex;

        const options = {
            network: "mainnet",
            expire_at: json.expires,
            nonce: json.nonce,
        };

        const address = "mmx1zmkneqqv28gyt8k8m3a09f2v2h3s0n44gly60hn5tq2d892re2asyh6g5x";
        const amount = "100001";
        const ask_currency = new addr_t().toString();
        const price = "0x4189374bc6a7f0";

        const tx = await Wallet.getOfferTradeTxAsync(ecdsaWallet, address, amount, ask_currency, price, options);

        assert.equal(tx.toString(), jsonTxt);

        const hash_serialize = tx.hash_serialize(true);
        assert.equal(hash_serialize.toHex(), hex);
    });
});
