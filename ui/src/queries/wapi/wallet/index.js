export * from "./mutations";

import axios from "@/queries/axios";
import { useQueryClient, keepPreviousData } from "@tanstack/vue-query";
import { useQueryWrapper as useQuery } from "@/composables/useQueryWrapper";
import { ONE_SECOND } from "../../common";

const getWalletAccounts = (signal) => axios.get("/wapi/wallet/accounts", { signal }).then((response) => response.data);
export const useWalletAccounts = () => {
    const queryClient = useQueryClient();
    return useQuery({
        queryKey: ["accounts"],
        queryFn: async ({ signal }) => {
            const data = await getWalletAccounts(signal);
            data?.forEach((account) => {
                queryClient.setQueryData(["accounts", "account", { index: account.account }], account);
            });
            return data;
        },
        gcTime: Infinity,
    });
};

export const prefetchWalletAccounts = (queryClient) => {
    queryClient.prefetchQuery({
        queryKey: ["accounts"],
        queryFn: ({ signal }) => getWalletAccounts(signal),
        staleTime: ONE_SECOND,
    });
};

const getWalletAccount = (params, signal) =>
    axios.get("/wapi/wallet/account", { params, signal }).then((response) => response.data);
export const useWalletAccount = (params, select) => {
    const queryClient = useQueryClient();

    const enabledFn = () => {
        const queryCache = queryClient.getQueryCache();
        const query = queryCache.find({ queryKey: ["accounts"] });
        return query && query.observers.length == 0;
    };

    return useQuery({
        queryKey: ["accounts", "account", params],
        queryFn: ({ signal }) => getWalletAccount(params, signal),
        enabled: enabledFn,
        select,
        gcTime: Infinity,
    });
};

const getWalletKeys = (params, signal) =>
    axios.get("/wapi/wallet/keys", { params, signal }).then((response) => response.data);
export const useWalletKeys = (params) => {
    return useQuery({
        queryKey: ["wallet", "keys", params],
        queryFn: ({ signal }) => getWalletKeys(params, signal),
    });
};

const getWalletAddress = (params, signal) =>
    axios.get("/wapi/wallet/address", { params, signal }).then((response) => response.data);
export const useWalletAddress = (params) => {
    return useQuery({
        queryKey: ["wallet", "address", params],
        queryFn: ({ signal }) => getWalletAddress(params, signal),
    });
};

const getWalletBalance = (params, signal) =>
    axios.get("/wapi/wallet/balance", { params, signal }).then((response) => response.data);
export const useWalletBalance = (params, select, enabled) => {
    return useQuery({
        queryKey: ["wallet", "balance", params],
        queryFn: ({ signal }) => getWalletBalance(params, signal),
        select: (data) => (select ? select(data) : data),
        placeholderData: keepPreviousData,
        enabled,
    });
};

const getWalletHistory = (params, signal) =>
    axios.get("/wapi/wallet/history", { params, signal }).then((response) => response.data);
export const useWalletHistory = (params, select) => {
    return useQuery({
        queryKey: ["wallet", "history", params],
        queryFn: ({ signal }) => getWalletHistory(params, signal),
        placeholderData: keepPreviousData,
        select: (data) => (select ? select(data) : data),
    });
};

const getWalletTokens = (signal) => axios.get("/wapi/wallet/tokens", { signal }).then((response) => response.data);
export const useWalletTokens = () => {
    return useQuery({
        queryKey: ["walletCfg", "tokens"],
        queryFn: ({ signal }) => getWalletTokens(signal),
    });
};

const getWalletTxHistory = (params, signal) =>
    axios.get("/wapi/wallet/tx_history", { params, signal }).then((response) => response.data);
export const useWalletTxHistory = (params) => {
    return useQuery({
        queryKey: ["wallet", "tx_history", params],
        queryFn: ({ signal }) => getWalletTxHistory(params, signal),
    });
};

const getWalletContracts = (params, signal) =>
    axios.get("/wapi/wallet/contracts", { params, signal }).then((response) => response.data);
export const useWalletContracts = (params, enabled) => {
    return useQuery({
        queryKey: ["wallet", "contracts", params],
        queryFn: ({ signal }) => getWalletContracts(params, signal),
        enabled: enabled ?? true,
    });
};

const getPlotnft = (params, signal) => axios.get("/wapi/plotnft", { params, signal }).then((response) => response.data);
export const usePlotnft = (params) => {
    return useQuery({
        queryKey: ["plotnft", params],
        queryFn: ({ signal }) => getPlotnft(params, signal),
    });
};

const getWalletPlots = (params, signal) =>
    axios.get("/wapi/wallet/plots", { params, signal }).then((response) => response.data);
export const useWalletPlots = (params) => {
    return useQuery({
        queryKey: ["wallet", "plots", params],
        queryFn: ({ signal }) => getWalletPlots(params, signal),
    });
};

const getWalletOffers = (params, signal) =>
    axios.get("/wapi/wallet/offers", { params, signal }).then((response) => response.data);
export const useWalletOffers = (params) => {
    return useQuery({
        queryKey: ["wallet", "offers", params],
        queryFn: ({ signal }) => getWalletOffers(params, signal),
        select: (data) => data.sort((L, R) => R.height - L.height),
        placeholderData: keepPreviousData,
    });
};

const getWalletSwapLiquid = (params, signal) =>
    axios.get("/wapi/wallet/swap/liquid", { params, signal }).then((response) => response.data);
export const useWalletSwapLiquid = (params) => {
    return useQuery({
        queryKey: ["wallet", "swap", "liquid", params],
        queryFn: ({ signal }) => getWalletSwapLiquid(params, signal),
    });
};
