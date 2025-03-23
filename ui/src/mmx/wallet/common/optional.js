export class optional {
    #value = undefined;
    constructor(value) {
        if (value !== null && value !== undefined) {
            this.#value = value;
        }
    }

    valueOf() {
        return this.#value;
    }

    // toString() {
    //     return this.#value.toString();
    // }
}
