import { u8, u32 } from "@noble/hashes/utils";
import { sha512, sha256 } from "@noble/hashes/sha2";
import { hmac } from "@noble/hashes/hmac";

import * as secp256k1 from "@noble/secp256k1";
secp256k1.etc.hmacSha256Sync = (k, ...m) => hmac(sha256, k, secp256k1.etc.concatBytes(...m));

import { addr_t } from "@/mmx/wallet/common/addr_t";
import "@/mmx/wallet/utils/Uint8ArrayUtils";

const KDF_ITERS = 4096;

const kdf_hmac_sha512 = (message, key, iterations) => {
    let tmp = new Uint8Array(key);
    for (let i = 0; i < iterations; ++i) {
        tmp = hmac.create(sha512, tmp).update(message).digest();
    }
    return tmp;
};

const hmac_sha512_n = (message, key, index) => {
    const indexArr = u8(new Uint32Array([index])).toReversed();
    const tmp = hmac.create(sha512, key).update(message).update(indexArr).digest();
    return tmp;
};

const getFarmerKey = (seed_value) => {
    const master = kdf_hmac_sha512(seed_value, sha256("MMX/farmer_keys"), KDF_ITERS);

    const tmp = hmac_sha512_n(master.first, master.second, 0);
    const pubKey = secp256k1.getPublicKey(tmp.first);
    return pubKey;
};

const getKeys = (seed_value, passphrase, index) => {
    const master = kdf_hmac_sha512(seed_value, sha256("MMX/seed/" + passphrase), KDF_ITERS);

    const chain = hmac_sha512_n(master.first, master.second, 11337);
    const account = hmac_sha512_n(chain.first, chain.second, 0);

    const tmp = hmac_sha512_n(account.first, account.second, index);
    const privKey = tmp.first;
    const pubKey = secp256k1.getPublicKey(privKey);
    return { privKey, pubKey };
};

const getAddress = (seed_value, passphrase, index) => {
    const { pubKey } = getKeys(seed_value, passphrase, index);
    const addr = sha256(pubKey);

    const addrStr = new addr_t(addr).toString();
    return addrStr;
};

const getFingerPrint = (seed_value, passphrase) => {
    let pass_hash = new Uint8Array(32);
    if (passphrase) {
        pass_hash = sha256("MMX/fingerprint/" + passphrase);
    }

    let hash = new Uint8Array(32);
    for (let i = 0; i < 16384; ++i) {
        hash = sha256(new Uint8Array([...hash, ...seed_value, ...pass_hash]));
    }

    const fingerPrint = u32(hash)[0];
    return fingerPrint;
};

const sign = (privKey, msg) => {
    return secp256k1.sign(msg, privKey).toCompactRawBytes();
};

// const signAsync = async (privKey, msg) => {
//     return (await secp256k1.signAsync(msg, privKey)).toCompactRawBytes();
// };

const syncFunctionList = { getFarmerKey, getAddress, getFingerPrint, getKeys };

export { getFarmerKey, getKeys, getAddress, getFingerPrint, sign, syncFunctionList };
