import { ChainParams } from "./ChainParams";

const chainParamsList = import.meta.glob(`@mmxConfig/(mainnet|mainnet-rc|testnet??)/chain/params.json`);
const chainExtraParamsList = import.meta.glob(`@mmxConfig/(mainnet|mainnet-rc|testnet??)/chain/params/(*)`, {
    as: "raw",
});

export const getChainParamsAsync = async (network) => {
    // get chain params
    const path = Object.keys(chainParamsList).filter((key) => key.endsWith(`${network}/chain/params.json`));
    const _chainParams = await chainParamsList[path]();

    // get chain extra params
    const extraPath = Object.keys(chainExtraParamsList).filter((key) => key.includes(`${network}/chain/params/`));

    for (const key of extraPath) {
        const param = key.match(/\/chain\/params\/(.*)$/)[1];
        const extraParam = (await chainExtraParamsList[key]()).trim();
        _chainParams[param] = extraParam;
    }

    const chainParams = new ChainParams(_chainParams);
    return chainParams;
};
