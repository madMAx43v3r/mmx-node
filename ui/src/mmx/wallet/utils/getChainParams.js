import { ChainParams } from "./ChainParams";

const chainParamsList = import.meta.glob("@mmxConfig/(mainnet|mainnet-rc|testnet??)/chain/params.json");

export const getChainParams = async (network) => {
    const path = Object.keys(chainParamsList).filter((key) => key.endsWith(`${network}/chain/params.json`));
    const _chainParams = await chainParamsList[path]();
    const chainParams = new ChainParams(_chainParams);
    return chainParams;
};
