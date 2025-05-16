import { promisify } from "./promisify";
import { getFarmerKey, getKeys, getAddress, getFingerPrint, sign, signAsync } from "./ECDSAUtils";

const getFingerPrintAsync = async (seed_value, passphrase) => await promisify("getFingerPrint", seed_value, passphrase);

const getFarmerKeyAsync = async (seed_value) => await promisify("getFarmerKey", seed_value);

const getAddressAsync = async (seed_value, passphrase, index) =>
    await promisify("getAddress", seed_value, passphrase, index);

const getKeysAsync = async (seed_value, passphrase, index) => promisify("getKeys", seed_value, passphrase, index);

export {
    getFarmerKey,
    getKeys,
    getAddress,
    getFingerPrint,
    sign,
    // ---
    getFingerPrintAsync,
    getFarmerKeyAsync,
    getAddressAsync,
    getKeysAsync,
    signAsync,
};
