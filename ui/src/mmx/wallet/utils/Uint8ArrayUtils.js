import { bytesToHex } from "@noble/hashes/utils";
import { abytes } from "@noble/hashes/_assert";

Object.defineProperty(Uint8Array.prototype, "first", {
    get() {
        abytes(this, 64);

        const length = this.length;
        const halfwayThrough = Math.floor(length / 2);
        const first = this.slice(0, halfwayThrough);
        return first;
    },
});

Object.defineProperty(Uint8Array.prototype, "second", {
    get() {
        abytes(this, 64);

        const length = this.length;
        const halfwayThrough = Math.floor(length / 2);
        const second = this.slice(halfwayThrough, this.length);
        return second;
    },
});

Uint8Array.prototype.toHex = function () {
    return bytesToHex(this).toUpperCase();
};
