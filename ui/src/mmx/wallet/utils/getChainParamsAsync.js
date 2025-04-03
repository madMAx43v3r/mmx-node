import { ChainParams } from "./ChainParams";

const chainParamsList = import.meta.glob(`@mmxConfig/(mainnet|mainnet-rc|testnet??)/chain/params.json`);
const chainExtraParamsList = import.meta.glob(`@mmxConfig/(mainnet|mainnet-rc|testnet??)/chain/params/(*)`, {
    query: "?raw",
    import: "default",
});

const chainParamsCache = new Map();
export const getChainParamsAsync = async (network) => {
    let chainParams = chainParamsCache.get(network);

    if (chainParams) {
        return chainParams;
    } else {
        // get chain params
        const path = Object.keys(chainParamsList).filter((key) => key.endsWith(`${network}/chain/params.json`));
        chainParams = await chainParamsList[path]();

        // get chain extra params
        const extraPath = Object.keys(chainExtraParamsList).filter((key) => key.includes(`${network}/chain/params/`));
        for (const key of extraPath) {
            const param = key.match(/\/chain\/params\/(.*)$/)[1];
            const extraParam = await chainExtraParamsList[key]();
            chainParams[param] = extraParam.trim();
        }

        const res = new ChainParams(chainParams);
        chainParamsCache.set(network, new ChainParams(chainParams));
        return res;
    }
};
