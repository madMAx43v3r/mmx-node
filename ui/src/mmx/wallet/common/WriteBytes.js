import { hexToBytes, isBytes, u8 } from "@noble/hashes/utils";

import { uint128 } from "./uint128";
import { bytes_t } from "./addr_t";
import { txin_t, txout_t } from "./txio_t";
import { optional } from "./optional";
import { Variant } from "./Variant";
import { WriteBuffer } from "./WriteBuffer";
import { pair } from "./pair";
import { vnxObject } from "./vnxObject";

import "../utils/Uint8ArrayUtils";

export class WriteBytes extends WriteBuffer {
    write_bytes_cstr(str) {
        const tmp = new TextEncoder().encode(str);
        this.write(tmp);
    }

    write_bytes_boolean(bool) {
        this.write(new Uint8Array([bool ? 1 : 0]));
    }

    write_bytes_string(str) {
        this.write_bytes_cstr("string<>");
        const tmp = new TextEncoder().encode(str);
        this.write_length(tmp);
        this.write(tmp);
    }

    write_bytes_int32(num) {
        if (!Number.isInteger(num) || num < -0x7fffffff - 1 || num > 0xffffffff) {
            throw new Error(`Invalid number: ${num}`);
        }
        this.write_bytes_int64(new Uint32Array([num]));
    }

    write_bytes_int64(bigNum) {
        const tmp = new BigInt64Array([bigNum]);
        this.write(tmp);
    }

    write_bytes_int128(bigNum) {
        const tmp = hexToBytes(bigNum.toString(16).padStart(32, "0")).toReversed();
        this.write(tmp);
    }

    write_bytes_bytes(bytes) {
        this.write_bytes_cstr("bytes<>");
        this.write_length(bytes);
        this.write(bytes);
    }

    write_length(bytes) {
        const tmp = u8(bytes);
        this.write_bytes(tmp.length);
    }

    write_bytes_variant(variant) {
        const value = variant.valueOf();
        if (value === undefined || value === null) {
            this.write_bytes_cstr("NULL");
        } else {
            this.write_bytes(value);
        }
    }

    write_bytes_optional(opt) {
        this.write_bytes_cstr("optional<>");
        if (opt.valueOf() != undefined) {
            this.write_bytes(true);
            this.write_bytes(opt.valueOf());
        } else {
            this.write_bytes(false);
        }
    }

    write_bytes_array(value, full_hash) {
        this.write_bytes_cstr("vector<>");
        this.write_bytes(value.length);
        for (let i = 0; i < value.length; i++) {
            this.write_bytes(value[i], full_hash);
        }
    }

    write_bytes_pair(value) {
        this.write_bytes_cstr("pair<>");
        this.write_bytes(value.key);
        this.write_bytes(value.value);
    }

    write_bytes_map(value) {
        let arr = [];
        value.forEach((value, key) => {
            arr.push(new pair(key, value));
        });
        this.write_bytes(arr);
    }

    write_bytes_txin_t(value, full_hash) {
        this.write_bytes_cstr("txin_t<>");
        this.write_bytes_ex(value);
        if (full_hash) {
            this.write_bytes(value.solution);
            this.write_bytes(value.flags);
        }
    }

    write_bytes_txout_t(value) {
        this.write_bytes_cstr("txout_t<>");
        this.write_bytes_ex(value);
    }

    write_bytes_ex(value) {
        this.write_bytes(value.address);
        this.write_bytes(value.contract);
        this.write_bytes_int128(value.amount);
        this.write_bytes(value.memo);
    }

    write_bytes_object(value) {
        this.write_bytes_cstr("object<>");
        this.write_bytes(value.field);
    }

    write_bytes_uint128(value) {
        this.write_bytes(value.lower());
        this.write_bytes(value.upper());
    }

    write_bytes(_value, full_hash) {
        let value = _value;
        if (typeof value === "boolean") {
            this.write_bytes_boolean(value);
        } else if (typeof value === "string") {
            this.write_bytes_string(value);
        } else if (typeof value === "number") {
            this.write_bytes_int32(value);
        } else if (typeof value === "bigint") {
            this.write_bytes_int64(value);
        } else if (typeof value === "object") {
            if (typeof value.getHashProxy === "function") {
                value = value.getHashProxy();
            }
            if (Array.isArray(value)) {
                this.write_bytes_array(value, full_hash);
            } else if (value instanceof Map) {
                this.write_bytes_map(value);
            } else if (value.constructor?.name && isBytes(value)) {
                this.write_bytes_bytes(value);
            } else if (value instanceof bytes_t) {
                this.write_bytes_bytes(value.data());
            } else if (value instanceof txin_t) {
                this.write_bytes_txin_t(value, full_hash);
            } else if (value instanceof txout_t) {
                this.write_bytes_txout_t(value);
            } else if (value instanceof optional) {
                this.write_bytes_optional(value);
            } else if (value instanceof Variant) {
                this.write_bytes_variant(value);
            } else if (value instanceof pair) {
                this.write_bytes_pair(value);
            } else if (value instanceof vnxObject) {
                this.write_bytes_object(value);
            } else if (value instanceof uint128) {
                this.write_bytes_uint128(value);
            } else {
                throw new Error(`Unknown object type`);
            }
        } else {
            throw new Error(`Unknown type: ${typeof value}`);
        }
    }

    write_field(field_name, field_value, full_hash) {
        this.write_bytes_cstr("field<>");
        this.write_bytes(field_name);
        if (field_value != undefined) {
            this.write_bytes(field_value, full_hash);
        }
    }
}
