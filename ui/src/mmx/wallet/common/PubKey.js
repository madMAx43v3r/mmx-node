import { bytes_t, hash_t } from "./addr_t";
import { WriteBytes } from "./WriteBytes";

class PubKey {
    #type_hash = BigInt("0xe47af6fcacfcefa5");

    __type = "mmx.solution.PubKey";

    version = 0;
    pubkey = "";
    signature = "";

    static hashHandler = {
        get: (target, prop) => {
            const value = target[prop];
            switch (prop) {
                case "pubkey":
                    return new bytes_t(value);
                case "signature":
                    return new bytes_t(value);
                default:
                    return value;
            }
        },
    };

    getHashProxy() {
        return new Proxy(this, PubKey.hashHandler);
    }

    constructor({ pubkey, signature }) {
        this.pubkey = pubkey;
        this.signature = signature;
    }

    hash_serialize(full_hash) {
        const hp = this.getHashProxy();

        const wb = new WriteBytes();
        wb.write_bytes(this.#type_hash);
        wb.write_field("version", hp.version);
        wb.write_field("pubkey", hp.pubkey);
        wb.write_field("signature", hp.signature);
        return wb.buffer;
    }

    calc_hash(full_hash) {
        const tmp = this.hash_serialize(full_hash);
        const hash = new hash_t(tmp).valueOf();
        return hash;
    }

    calc_cost(params) {
        return BigInt(params.min_txfee_sign);
    }
}

export { PubKey };
