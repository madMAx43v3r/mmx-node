import { bech32m } from "@scure/base";

export const validateAddress = (address) => {
    try {
        const decoded = bech32m.decode(address);
        if (decoded.words.length != 52) {
            // (size != 52)
            return false;
        }
    } catch (e) {
        // invalid address
        return false;
    }
    return true;
};
