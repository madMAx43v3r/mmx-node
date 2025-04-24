import { bech32m } from "@scure/base";
import { hexToBytes, isBytes, u8 } from "@noble/hashes/utils";

export class bytes_t {
    #bytes = new Uint8Array();
    constructor(bytes) {
        if (isBytes(bytes)) {
            this.#setBytes(bytes);
        } else if (typeof bytes == "string") {
            this.#setBytes(hexToBytes(bytes));
        }
    }

    #setBytes(_bytes) {
        const bytes = u8(_bytes);
        this.#bytes = bytes;
    }

    valueOf() {
        return this.#bytes;
    }

    toString() {
        return this.valueOf().toHex();
    }
}

export class addr_t extends bytes_t {
    static prefix = "mmx";
    constructor(addr) {
        if (typeof addr == "string") {
            const decoded = bech32m.decodeToBytes(addr);
            const { prefix, bytes } = decoded;
            if (prefix != addr_t.prefix) {
                throw new Error("Invalid address prefix");
            }
            super(bytes.toReversed());
        } else if (isBytes(addr)) {
            if (addr.length != 32) throw new Error("Invalid address length");
            super(addr);
        } else if (addr == undefined) {
            super(new Uint8Array(32));
        } else {
            throw new Error(`Invalid address type ${typeof addr}`);
        }
    }

    toString() {
        return bech32m.encodeFromBytes(addr_t.prefix, this.valueOf().toReversed());
    }
}

import { sha256 } from "@noble/hashes/sha256";
export class hash_t extends bytes_t {
    constructor(data) {
        if (data == undefined) {
            super(new Uint8Array(32));
        } else {
            super(sha256(data));
        }
    }
}
