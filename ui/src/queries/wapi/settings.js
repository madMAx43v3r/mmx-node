import axios from "@/queries/axios";
import { useMutation, useQueryClient } from "@tanstack/vue-query";
import { useQueryWrapper as useQuery } from "@/composables/useQueryWrapper";

import { ONE_SECOND, pushConfigSuccess, pushSuccess, onError } from "../common";

const getConfig = (signal) => axios.get("/wapi/config/get", { signal }).then((response) => response.data);
export const useConfig = () => {
    return useQuery({
        queryKey: ["config"],
        queryFn: ({ signal }) => getConfig(signal),
        gcTime: Infinity,
    });
};

/*
const getConfigKey = (params, signal) =>
    axios.get("/wapi/config/get", { params, signal }).then((response) => response.data);
export const useConfigKey = (params) => {
    return useQuery({
        queryKey: ["config", "get", params],
        queryFn: ({ signal }) => getConfigKey(params, signal),
    });
};
*/

const setConfig = (payload) => axios.post("/wapi/config/set", payload).then((response) => response.data);
export const useSetConfig = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: ({ key, value, tmp_only }) => {
            return setConfig({ key, value, tmp_only });
        },
        onSuccess: (result, variables) => {
            queryClient.invalidateQueries({ queryKey: ["config"] });
            if (!variables.suppressNotification) {
                pushConfigSuccess(variables);
            }
        },
        onError,
    });
};

const getChainInfo = (signal) => axios.get("/wapi/chain/info", { signal }).then((response) => response.data);
export const useChainInfo = () => {
    return useQuery({
        queryKey: ["chain", "info"],
        queryFn: ({ signal }) => getChainInfo(signal),
        gcTime: Infinity,
    });
};
