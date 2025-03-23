import axios from "@/queries/axios";
import { useMutation, useQueryClient } from "@tanstack/vue-query";
import { useQueryWrapper as useQuery } from "@/composables/useQueryWrapper";

import { prefetchWalletAccounts } from "@/queries/wapi";

import { ONE_SECOND, pushConfigSuccess, pushSuccess, onError } from "../common";

const getPeerInfo = (signal) => axios.get("/api/router/get_peer_info", { signal }).then((response) => response.data);
export const usePeers = () => {
    return useQuery({
        queryKey: ["peers"],
        queryFn: ({ signal }) => getPeerInfo(signal),
        select: (data) => data.peers,
        refetchInterval: 5.1 * ONE_SECOND,
    });
};

export const usePeerInfoMutation = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: () => getPeerInfo(),
        onSuccess: (data) => {
            queryClient.setQueryData(["peers"], data);
        },
    });
};

const kickPeer = (address) => axios.post("/api/router/kick_peer", { address }).then((response) => response.data);
export const useKickPeer = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (address) => kickPeer(address),
        onSuccess: (data, address) => {
            queryClient.invalidateQueries({ queryKey: ["peers"] });
            pushSuccess({
                message: "Peer kicked: " + address, //TODO i18n
            });
        },
    });
};

const revertSync = (payload) => axios.post("/api/node/revert_sync", payload).then((response) => response.data);
export const useRevertSync = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => revertSync(payload),
        onSuccess: (data, variables) => {
            queryClient.invalidateQueries({ queryKey: ["node"] });
            queryClient.invalidateQueries({ queryKey: ["config"] });
            pushConfigSuccess({ key: "Node.revert_sync", value: variables.height, restart: false });
        },
    });
};

const addPlotDir = (path) => axios.post("/api/harvester/add_plot_dir", { path }).then((response) => response.data);
export const useAddPlotDir = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (path) => {
            return addPlotDir(path);
        },
        onSuccess: (data, variables) => {
            queryClient.invalidateQueries({ queryKey: ["config"] });
            pushConfigSuccess({ key: "Harvester.plot_dirs", value: "+ " + variables, restart: false });
        },
        onError,
    });
};

const removePlotDir = (path) => axios.post("/api/harvester/rem_plot_dir", { path }).then((response) => response.data);
export const useRemovePlotDir = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (path) => {
            return removePlotDir(path);
        },
        onSuccess: (data, variables) => {
            queryClient.invalidateQueries({ queryKey: ["config"] });
            pushConfigSuccess({ key: "Harvester.plot_dirs", value: "- " + variables, restart: false });
        },
        onError,
    });
};

const addTokens = (address) => axios.post("/api/wallet/add_token", { address }).then((response) => response.data);
export const useAddTokens = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (address) => {
            return addTokens(address);
        },
        onSuccess: (data, variables) => {
            queryClient.invalidateQueries({ queryKey: ["walletCfg", "tokens"] });
            pushSuccess({
                message: "Added token to whitelist: " + variables,
            }); //TODO i18n
        },
        onError,
    });
};

const removeTokens = (address) => axios.post("/api/wallet/rem_token", { address }).then((response) => response.data);
export const useRemoveTokens = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (address) => {
            return removeTokens(address);
        },
        onSuccess: (data, variables) => {
            queryClient.invalidateQueries({ queryKey: ["walletCfg", "tokens"] });
            pushSuccess({
                message: "Removed token from whitelist: " + variables, //TODO i18n
            });
        },
        onError,
    });
};

const harvesterReload = () => axios.post("/api/harvester/reload").then((response) => response.data);
export const useHarvesterReload = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: () => harvesterReload(),
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ["farm"] });
            pushSuccess({
                message: "Plots reloaded", //TODO i18n
            });
        },
    });
};

const getMnemonicWordList = (signal) =>
    axios.get("/api/wallet/get_mnemonic_wordlist", { signal }).then((response) => response.data);
export const useMnemonicWordList = () => {
    return useQuery({
        queryKey: ["walletCfg", "mnemonic_wordlist"],
        queryFn: ({ signal }) => getMnemonicWordList(signal),
    });
};

const createWallet = (payload) => axios.post("/api/wallet/create_wallet", payload).then((response) => response.data);
export const useCreateWallet = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => createWallet(payload),
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ["accounts"] });
            //prefetchWalletAccounts(queryClient);
            pushSuccess({
                message: "Wallet created", //TODO i18n
            });
        },
        onError,
    });
};

const removeAccount = (params) => axios.get("/api/wallet/remove_account", { params }).then((response) => response.data);
export const useRemoveAccount = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (params) => removeAccount(params),
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ["accounts"] });
            //prefetchWalletAccounts(queryClient);
            pushSuccess({
                message: "Wallet removed", //TODO i18n
            });
        },
        onError,
    });
};

const resetCache = (index) => axios.post("/api/wallet/reset_cache", { index }).then((response) => response.data);
export const useResetCache = () => {
    //const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (index) => resetCache(index),
        onSuccess: () => {
            pushSuccess({
                message: "Cache reset successfully", //TODO i18n
            });
        },
        onError,
    });
};

const setAddressCount = (payload) =>
    axios.post("/api/wallet/set_address_count", payload).then((response) => response.data);
export const useSetAddressCount = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => setAddressCount(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccess({
                message: "Updated successfully", //TODO i18n
            });
        },
        onError,
    });
};

const getWalletIsLocked = (params, signal) =>
    axios.get("/api/wallet/is_locked", { params, signal }).then((response) => response.data);
const getWalletIsLockedQueryOptions = (params) => ({
    queryKey: ["wallet", "is_locked", params],
    queryFn: ({ signal }) => getWalletIsLocked(params, signal),
});
export const useWalletIsLocked = (params, enabled) => {
    return useQuery({
        ...getWalletIsLockedQueryOptions(params),
        enabled,
    });
};

export const fetchWalletIsLocked = async (queryClient, params) => {
    return await queryClient.fetchQuery(getWalletIsLockedQueryOptions(params));
};

export const ensureWalletIsLocked = async (queryClient, params) => {
    return await queryClient.ensureQueryData(getWalletIsLockedQueryOptions(params));
};

const walletLock = (params) => axios.post("/api/wallet/lock", params).then((response) => response.data);
export const useWalletLock = (params) => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: () => walletLock(params),
        onSuccess: () => {
            queryClient.invalidateQueries({
                queryKey: ["wallet", "is_locked", params],
            });
            pushSuccess({
                message: `Wallet #${params.index} locked`, // TODO i18n
            });
        },
        onError,
    });
};

const walletUnlock = (payload) => axios.post("/api/wallet/unlock", payload).then((response) => response.data);
export const useWalletUnlock = (params) => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (passphrase) => walletUnlock({ ...params, passphrase }),
        onSuccess: () => {
            queryClient.invalidateQueries({
                queryKey: ["wallet", "is_locked", params],
            });
            pushSuccess({
                message: `Wallet #${params.index} unlocked`, // TODO i18n
            });
        },
        onError,
    });
};
