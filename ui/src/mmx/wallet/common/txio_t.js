import { addr_t } from "./addr_t";
import { optional } from "./optional";

class txio_t {
    static MAX_MEMO_SIZE = 64;
    __type = "mmx.txio_t";

    address = "";
    contract = "";
    amount = 0;
    memo = null;

    static hashHandler = {
        get: (target, prop) => {
            const value = target[prop];
            switch (prop) {
                case "address":
                    return new addr_t(value);
                case "contract":
                    return new addr_t(value);
                case "amount":
                    return BigInt(value);
                case "memo":
                    return new optional(value);
                default:
                    return value;
            }
        },
    };

    getHashProxy() {
        return new Proxy(this, txio_t.hashHandler);
    }

    constructor({ address, contract, amount, memo }) {
        this.address = address;
        this.contract = contract;
        this.amount = amount ?? this.amount;
        this.memo = memo ?? this.memo;
    }

    calc_cost(params) {
        if (!params) {
            throw new Error("chain params is required");
        }
        let cost = BigInt(params.min_txfee_io);

        cost += this.memo ? BigInt(Math.floor((this.memo.length + 31) / 32)) * BigInt(params.min_txfee_memo) : 0n;
        return cost;
    }
}

class txin_t extends txio_t {
    static NO_SOLUTION = -1;
    static IS_EXEC = 1;

    __type = "mmx.txin_t";

    solution = -1;
    flags = 0;

    constructor({ address, contract, amount, memo, solution, flags }) {
        super({ address, contract, amount, memo });
        this.solution = solution ?? this.solution;
        this.flags = flags ?? this.flags;
    }
}
class txout_t extends txio_t {}

export { txin_t, txout_t, txio_t };
