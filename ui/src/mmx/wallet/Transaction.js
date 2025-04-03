import * as secp256k1 from "@noble/secp256k1";
const { randomBytes, bytesToNumberBE } = secp256k1.etc;

import { JSONbigNative } from "./utils/JSONbigNative";

import { tx_note_e } from "./common/tx_note_e";
import { addr_t, bytes_t, hash_t } from "./common/addr_t";
import { txin_t, txout_t, txio_t } from "./common/txio_t";
import { optional } from "./common/optional";
import { PubKey } from "./common/PubKey";
import { Operation } from "./common/Operation";

import { WriteBytes } from "./common/WriteBytes";
import { exec_result_t } from "./common/exec_result_t";
import { Executable } from "./common/Contract";
import { ChainParams } from "./utils/ChainParams";

class Transaction {
    #type_hash = BigInt("0xce0462acdceaa5bc");

    __type = "mmx.Transaction";
    id = new hash_t().toString();

    version = 0;
    expires = 4294967295;
    fee_ratio = 1024;
    static_cost = 0;
    max_fee_amount = 0;

    note = 0;
    nonce = 0n;
    network = "";

    sender = null;

    inputs = [];
    outputs = [];

    execute = [];
    solutions = [];

    deploy = null;
    exec_result = null;

    constructor() {}

    static parse(json) {
        const obj = JSONbigNative.parse(json);
        const tx = Transaction.cast(obj);
        return tx;
    }

    static cast(obj) {
        const tx = Object.assign(new Transaction(), obj);
        return tx;
    }

    #stringify(...options) {
        return JSONbigNative.stringify(this, ...options);
    }

    toString(...options) {
        return this.#stringify(...options);
    }

    static hashHandler = {
        get: (target, prop) => {
            const value = target[prop];
            switch (prop) {
                case "note":
                    return tx_note_e[value];
                case "sender":
                    return new optional(value ? new addr_t(value) : null);
                case "inputs":
                    return value.map((i) => new txin_t(i));
                case "outputs":
                    return value.map((i) => new txout_t(i));
                case "execute":
                    return value.map((i) => new Operation(i));
                case "solutions":
                    return value.map((i) => new PubKey(i));
                case "deploy":
                    return value ? new Executable(value) : null;
                case "exec_result":
                    return new optional(value !== null ? new exec_result_t(value) : null);
                default:
                    return value;
            }
        },
    };

    getHashProxy() {
        return new Proxy(this, Transaction.hashHandler);
    }

    hash_serialize(full_hash) {
        const hp = this.getHashProxy();

        const wb = new WriteBytes();
        wb.write_bytes(this.#type_hash);
        wb.write_field("version", hp.version);
        wb.write_field("expires", hp.expires);
        wb.write_field("fee_ratio", hp.fee_ratio);
        wb.write_field("max_fee_amount", hp.max_fee_amount);
        wb.write_field("note", hp.note);
        wb.write_field("nonce", hp.nonce);
        wb.write_field("network", hp.network);

        wb.write_field("sender", hp.sender);

        wb.write_field("inputs", hp.inputs, full_hash);
        wb.write_field("outputs", hp.outputs);

        wb.write_field("execute");
        wb.write_bytes(hp.execute.length);

        for (const op of hp.execute) {
            //write_bytes(out, op ? op->calc_hash(full_hash) : hash_t());
            wb.write_bytes(new bytes_t(op.calc_hash(full_hash)));
        }

        wb.write_field("deploy", hp.deploy ? new bytes_t(hp.deploy.calc_hash(full_hash)) : new hash_t());

        if (full_hash) {
            wb.write_field("static_cost", hp.static_cost);
            wb.write_field("solutions");
            wb.write_bytes(hp.solutions.length);

            for (const sol of hp.solutions) {
                wb.write_bytes(new bytes_t(sol.calc_hash()));
            }

            wb.write_field(
                "exec_result",
                hp.exec_result.valueOf() ? new bytes_t(hp.exec_result.valueOf().calc_hash()) : new hash_t()
            );
        }

        return wb.buffer;
    }

    calc_hash(full_hash) {
        const tmp = this.hash_serialize(full_hash);
        const hash = new hash_t(tmp).valueOf();
        return hash;
    }

    finalizeId() {
        this.id = this.calc_hash().toHex();
    }

    finalize() {
        while (!this.nonce) {
            this.nonce = bytesToNumberBE(randomBytes(8));
        }
        this.finalizeId();
    }

    add_output(currency, dst_addr, amount, memo) {
        if (memo && memo.length > txio_t.MAX_MEMO_SIZE) {
            throw new Error("memo too long");
        }
        const obj = {
            address: dst_addr,
            contract: currency,
            amount,
            memo,
        };
        this.outputs.push(obj);
    }

    calc_cost(_params) {
        const params = new ChainParams(_params);

        const hp = this.getHashProxy();

        let cost = BigInt(params.min_txfee);
        cost += BigInt(this.execute.length) * BigInt(params.min_txfee_exec);

        if (hp.exec_result.valueOf()) {
            cost += hp.exec_result.valueOf().calc_cost(params);
        }

        cost += hp.inputs.reduce((acc, input) => {
            acc += input.calc_cost(params);
            return acc;
        }, 0n);

        cost += hp.outputs.reduce((acc, output) => {
            acc += output.calc_cost(params);
            return acc;
        }, 0n);

        cost += hp.execute.reduce((acc, op) => {
            acc += op.calc_cost(params);
            return acc;
        }, 0n);

        cost += hp.solutions.reduce((acc, solution) => {
            acc += solution.calc_cost(params);
            return acc;
        }, 0n);

        if (hp.deploy) {
            cost += BigInt(params.min_txfee_deploy) + hp.deploy.calc_cost(params);
        }

        if (cost >> 64n) {
            throw new Error("tx cost amount overflow");
        }

        return cost;
    }

    // private/hidden field to store aux info(like feeValue)
    #aux = {};
    get aux() {
        return this.#aux;
    }
    set aux(value) {
        this.#aux = value;
    }
}

export { Transaction };
