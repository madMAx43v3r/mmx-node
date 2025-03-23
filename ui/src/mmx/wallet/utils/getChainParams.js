const chainParamsList = import.meta.glob("../../config/(mainnet|mainnet-rc|testnet??)/chain/params.json");

export const getChainParams = async (network) => {
    const chainParams = await chainParamsList[`../../config/${network}/chain/params.json`]();
    return chainParams;
};
