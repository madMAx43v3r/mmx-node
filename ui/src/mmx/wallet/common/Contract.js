import { WriteBytes } from "./WriteBytes";
import { addr_t, hash_t } from "./addr_t";
import { get_num_bytes } from "./utils";
import { Variant } from "./Variant";

class Contract {
    // eslint-disable-next-line no-unused-private-class-members
    #type_hash = BigInt("0x26b896ae8c415285");
    __type = "mmx.Contract";

    version = 0;

    constructor(params) {
        this.version = params.version;
    }

    // calc_cost(params) {
    //     if (!params) {
    //         throw new Error("chain params is required");
    //     }

    //     const cost = BigInt(this.num_bytes()) * BigInt(params.min_txfee_byte);
    //     return cost;
    // }

    num_bytes(total) {
        return 16;
    }
}

class TokenBase extends Contract {
    // eslint-disable-next-line no-unused-private-class-members
    #type_hash = BigInt("0x5aeed4c96d232b5e");
    __type = "mmx.contract.TokenBase";

    name = "";
    symbol = "";
    decimals = 0;
    meta_data = null;

    static hashHandler = {
        get: (target, prop) => {
            const value = target[prop];
            switch (prop) {
                case "meta_data":
                    return new Variant(value);
                default:
                    return value;
            }
        },
    };

    getHashProxy() {
        return new Proxy(this, TokenBase.hashHandler);
    }

    constructor(params) {
        super(params);
        this.name = params.name;
        this.symbol = params.symbol;
        this.decimals = params.decimals;
        this.meta_data = params.meta_data;
    }

    num_bytes() {
        const hp = this.getHashProxy();
        return super.num_bytes() + this.name.length + this.symbol.length + get_num_bytes(hp.meta_data);
    }
}

class Executable extends TokenBase {
    #type_hash = BigInt("0xfa6a3ac9103ebb12");
    __type = "mmx.contract.Executable";

    binary = "";
    init_method = "init";
    init_args = [];
    depends = "";

    constructor(params) {
        super(params);

        this.binary = params.binary;
        this.init_method = params.init_method;
        this.init_args = params.init_args;
        this.depends = params.depends;
    }

    static hashHandler = {
        get: (target, prop) => {
            const value = target[prop];
            switch (prop) {
                case "binary":
                    return new addr_t(value);
                case "init_args":
                    return value.map((i) => new Variant(i));
                case "depends":
                    if (value.length > 0) {
                        throw new Error("Not implemented");
                    }
                    return new Map();
                default:
                    return TokenBase.hashHandler.get(target, prop);
            }
        },
    };

    getHashProxy() {
        return new Proxy(this, Executable.hashHandler);
    }

    hash_serialize(full_hash) {
        const hp = this.getHashProxy();
        const wb = new WriteBytes();

        wb.write_bytes(this.#type_hash);
        wb.write_field("version", hp.version);
        wb.write_field("name", hp.name);
        wb.write_field("symbol", hp.symbol);
        wb.write_field("decimals", hp.decimals);
        wb.write_field("meta_data", hp.meta_data);
        wb.write_field("binary", hp.binary);
        wb.write_field("init_method", hp.init_method);
        wb.write_field("init_args", hp.init_args);
        wb.write_field("depends", hp.depends);

        return wb.buffer;
    }

    calc_hash(full_hash) {
        const tmp = this.hash_serialize(full_hash);
        const hash = new hash_t(tmp).data();
        return hash;
    }

    num_bytes() {
        const hp = this.getHashProxy();

        let sum = super.num_bytes() + 32 + this.init_method.length + hp.depends.size * 32;
        hp.init_args.map((arg) => (sum += get_num_bytes(arg)));
        hp.depends.forEach((value, key) => (sum += key.length));
        return sum;
    }

    calc_cost(params) {
        if (!params) {
            throw new Error("chain params is required");
        }
        const hp = this.getHashProxy();
        const cost =
            BigInt(this.num_bytes()) * BigInt(params.min_txfee_byte) +
            BigInt(hp.depends.size) * BigInt(params.min_txfee_depend);
        return cost;
    }
}
export { Contract, Executable };
