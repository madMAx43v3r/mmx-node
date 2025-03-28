export class spend_options_t {
    auto_send = true;
    mark_spent = false;
    fee_ratio = 1024;
    gas_limit = 5000000;

    // expire_at = null;
    // expire_delta = null;
    // nonce = null;
    // user = null;
    // sender = null;
    // passphrase = null;
    // note = null;
    // memo = null;
    // owner_map = null;
    // contract_map = null;

    constructor(options) {
        if (!options) {
            throw new Error("options is required");
        }

        if (!options.network) {
            throw new Error("network is required");
        }

        if (!options.expire_at) {
            throw new Error("expire_at is required");
        }

        Object.assign(this, options);
    }
}
