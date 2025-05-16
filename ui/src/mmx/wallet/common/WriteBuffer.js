import { u8 } from "@noble/hashes/utils";

const concatenateUint8Arrays = (uint8arrays) => {
    const totalLength = uint8arrays.reduce((total, uint8array) => total + uint8array.byteLength, 0);

    const result = new Uint8Array(totalLength);

    let offset = 0;
    uint8arrays.forEach((uint8array) => {
        result.set(uint8array, offset);
        offset += uint8array.byteLength;
    });

    return result;
};

export class WriteBuffer {
    #chunks = [];
    #buffer = new Uint8Array([]);
    get buffer() {
        this.flush();
        return this.#buffer;
    }

    flush() {
        if (this.#chunks.length > 0) {
            const tmp = this.#chunks.flat();
            this.#buffer = concatenateUint8Arrays([this.#buffer, ...tmp]);
            this.#chunks = [];
        }
    }

    write(buffer) {
        this.#chunks.push(u8(buffer));
    }
}
