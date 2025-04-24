import { Variant } from "./Variant";

export class vnxObject {
    get field() {
        const entries = Object.entries(this).map(([key, value]) => [key, new Variant(value)]);
        return new Map(entries);
    }
    constructor(obj) {
        Object.assign(this, obj);
    }
}
