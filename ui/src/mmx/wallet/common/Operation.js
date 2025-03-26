import { addr_t, hash_t } from "./addr_t";
import { optional } from "./optional";
import { Variant } from "./Variant";
import { uint128 } from "./uint128";

import { WriteBytes } from "./WriteBytes";

class Operation {
    constructor(op) {
        if (op.__type === "mmx.operation.Execute") {
            return new Execute(op);
        } else if (op.__type === "mmx.operation.Deposit") {
            return new Deposit(op);
        } else {
            throw new Error("Invalid Operation type");
        }
    }
}

// cpp implementation: /src/operation/Execute.cpp
class Execute {
    #type_hash = BigInt("0x8cd9012d9098c1d1");

    __type = "mmx.operation.Execute";
    version = 0;
    address = "";
    solution = 0;
    method = "";
    args = [];
    user = "";

    static hashHandler = {
        get: (target, prop) => {
            const value = target[prop];
            switch (prop) {
                case "address":
                    return new addr_t(value);
                case "args":
                    return value.map((i) => new Variant(i));
                case "user":
                    return new optional(value ? new addr_t(value) : null);
                default:
                    return value;
            }
        },
    };

    getHashProxy() {
        return new Proxy(this, Execute.hashHandler);
    }

    constructor({ address, solution, method, args, user }) {
        this.address = address;
        this.solution = solution;
        this.method = method;
        this.args = args;
        this.user = user;
    }

    hash_serialize(full_hash) {
        const hp = this.getHashProxy();
        const wb = new WriteBytes();

        wb.write_bytes(this.#type_hash);
        wb.write_field("version", hp.version);
        wb.write_field("address", hp.address);
        wb.write_field("method", hp.method);
        wb.write_field("args", hp.args);
        wb.write_field("user", hp.user);

        if (full_hash) {
            wb.write_field("solution", hp.solution);
        }

        return wb.buffer;
    }

    calc_hash(full_hash) {
        const tmp = this.hash_serialize(full_hash);
        const hash = new hash_t(tmp).data();
        return hash;
    }

    calc_cost(params) {
        let payload = this.method.length;

        const hp = this.getHashProxy();
        hp.args.reduce((acc, arg) => {
            acc += arg.size();
            return acc;
        }, payload);

        return 0n + BigInt(payload) * BigInt(params.min_txfee_byte);
    }
}

// cpp implementation: /src/operation/Deposit.cpp
class Deposit extends Execute {
    #type_hash = BigInt("0xc23408cb7b04b0ec");
    __type = "mmx.operation.Deposit";

    currency = null;
    amount = 0;

    constructor({ address, solution, method, args, user, currency, amount }) {
        super({ address, solution, method, args, user });
        this.currency = currency;
        this.amount = amount;
    }

    static hashHandler = {
        get: (target, prop) => {
            const value = target[prop];
            switch (prop) {
                case "currency":
                    return new addr_t(value);
                case "amount":
                    return new uint128(value);
                default:
                    return Execute.hashHandler.get(target, prop);
            }
        },
    };

    getHashProxy() {
        return new Proxy(this, Deposit.hashHandler);
    }

    hash_serialize(full_hash) {
        const hp = this.getHashProxy();
        const wb = new WriteBytes();

        wb.write_bytes(this.#type_hash);
        wb.write_field("version", hp.version);
        wb.write_field("address", hp.address);
        wb.write_field("method", hp.method);
        wb.write_field("args", hp.args);
        wb.write_field("user", hp.user);

        wb.write_field("currency", hp.currency);
        wb.write_field("amount", hp.amount);

        if (full_hash) {
            wb.write_field("solution", hp.solution);
        }

        return wb.buffer;
    }

    calc_hash(full_hash) {
        const tmp = this.hash_serialize(full_hash);
        const hash = new hash_t(tmp).data();
        return hash;
    }
}

export { Operation };
