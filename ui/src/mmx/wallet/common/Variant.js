import { WriteBuffer } from "./WriteBuffer";

const CODE_NULL = 0; ///< used for nullptr or empty Variant

const CODE_UINT8 = 1; ///< 8-bit unsigned integer
const CODE_UINT16 = 2; ///< 16-bit unsigned integer
const CODE_UINT32 = 3; ///< 32-bit unsigned integer
const CODE_UINT64 = 4; ///< 64-bit unsigned integer

const CODE_INT8 = 5; ///< 8-bit signed integer
const CODE_INT16 = 6; ///< 16-bit signed integer
const CODE_INT32 = 7; ///< 32-bit signed integer
const CODE_INT64 = 8; ///< 64-bit signed integer

//const CODE_ARRAY = 11;
const CODE_LIST = 12;
//const CODE_MAP = 13;
const CODE_DYNAMIC = 17;

//const CODE_TUPLE = 23;
const CODE_OBJECT = 24;

const CODE_BOOL = 31;
const CODE_STRING = 32;

const VNX_MAX_SIZE = 0xffffffff;

const max_recursion = 100;
export class Variant {
    #wb = new WriteBuffer();
    get data() {
        return this.#wb.buffer;
    }

    size() {
        return this.#wb.buffer.byteLength;
    }

    value = null;
    valueOf() {
        return this.value;
    }

    constructor(value, call_depth = 0) {
        if (++call_depth > max_recursion) {
            throw new Error("recursion too deep");
        }

        this.value = value;

        if (value === null || value === undefined) {
            const code = CODE_NULL;
            const header = new Uint16Array([1, code]);
            this.#wb.write(header);

            const data = new Uint32Array([]);
            this.#wb.write(data);
        } else if (typeof value === "bigint" || (typeof value === "number" && Number.isInteger(value))) {
            let code;
            let data;
            const bitSize = this.bitSize(value);
            if (value >= 0) {
                if (bitSize <= 8) {
                    code = CODE_UINT8;
                    data = new Uint8Array([Number(value)]);
                } else if (bitSize <= 16) {
                    code = CODE_UINT16;
                    data = new Uint16Array([Number(value)]);
                } else if (bitSize <= 32) {
                    code = CODE_UINT32;
                    data = new Uint32Array([Number(value)]);
                } else {
                    code = CODE_UINT64;
                    data = new BigUint64Array([value]);
                }
            } else {
                if (bitSize <= 8 + 1) {
                    code = CODE_INT8;
                    data = new Int8Array([Number(value)]);
                } else if (bitSize <= 16 + 1) {
                    code = CODE_INT16;
                    data = new Int16Array([Number(value)]);
                } else if (bitSize <= 32 + 1) {
                    code = CODE_INT32;
                    data = new Int32Array([Number(value)]);
                } else {
                    code = CODE_INT64;
                    data = new BigInt64Array([BigInt(value)]);
                }
            }
            const header = new Uint16Array([1, code]);
            this.#wb.write(header);

            this.#wb.write(data);
        } else if (typeof value === "boolean") {
            const code = CODE_BOOL;
            const header = new Uint16Array([1, code]);
            this.#wb.write(header);

            const data = new Uint8Array([value]);
            this.#wb.write(data);
        } else if (typeof value === "string") {
            // if (value.length > VNX_MAX_SIZE) {
            //     // this never throws, because max possible string length in JS (2^30 - 2) < VNX_MAX_SIZE (2^32 - 1)
            //     throw new Error("write(std::string): size > VNX_MAX_SIZE");
            // }
            const code = CODE_STRING;
            const header = new Uint16Array([1, code]);
            this.#wb.write(header);

            const strData = new TextEncoder().encode(value);
            const length = new Uint32Array([strData.length]);
            this.#wb.write(length);
            this.#wb.write(strData);
        } else if (Array.isArray(value)) {
            const code = CODE_LIST;

            const header = new Uint16Array([2, code, CODE_DYNAMIC]);
            this.#wb.write(header);

            const length = new Uint32Array([value.length]);
            this.#wb.write(length);

            value.forEach((item) => {
                const data = new Variant(item, call_depth).data;
                this.#wb.write(data);
            });
        } else if (typeof value === "object") {
            const code = CODE_OBJECT;

            const header = new Uint16Array([1, code]);
            this.#wb.write(header);

            const objKeys = Object.keys(value);
            const length = new Uint32Array([objKeys.length]);
            this.#wb.write(length);

            objKeys
                .sort((a, b) => a.localeCompare(b))
                .forEach((keyName) => {
                    const strData = new TextEncoder().encode(keyName);
                    const length = new Uint32Array([strData.length]);
                    this.#wb.write(length);
                    this.#wb.write(strData);

                    const data = new Variant(value[keyName], call_depth).data;
                    this.#wb.write(data);
                });
        } else {
            throw new Error("Unsupported type", typeof value);
        }
    }

    bitSize(num) {
        return num.toString(2).length;
    }
}
