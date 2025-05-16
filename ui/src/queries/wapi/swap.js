import axios from "@/queries/axios";
import { useMutation, useQueryClient, keepPreviousData } from "@tanstack/vue-query";
import { useQueryWrapper as useQuery } from "@/composables/useQueryWrapper";
import { ONE_SECOND, onError, pushSuccess, pushError } from "../common";
import i18n from "@/plugins/i18n";
const { t } = i18n.global;

const getSwapList = (params, signal) =>
    axios.get("/wapi/swap/list", { params, signal }).then((response) => response.data);
export const useSwapList = (params) => {
    return useQuery({
        queryKey: ["swap", "list", params],
        queryFn: ({ signal }) => getSwapList(params, signal),
        placeholderData: keepPreviousData,
    });
};

const getSwapInfo = (params, signal) =>
    axios.get("/wapi/swap/info", { params, signal }).then((response) => response.data);
export const useSwapInfo = (params) => {
    return useQuery({
        queryKey: ["swap", "info", params],
        queryFn: ({ signal }) => getSwapInfo(params, signal),
    });
};

const getSwapHistory = (params, signal) =>
    axios.get("/wapi/swap/history", { params, signal }).then((response) => response.data);
export const useSwapHistory = (params) => {
    return useQuery({
        queryKey: ["swap", "history", params],
        queryFn: ({ signal }) => getSwapHistory(params, signal),
    });
};

const getSwapUserInfo = (params, signal) =>
    axios.get("/wapi/swap/user_info", { params, signal }).then((response) => response.data);
export const useSwapUserInfo = (params, enabled) => {
    return useQuery({
        queryKey: ["swap", "user_info", params],
        queryFn: ({ signal }) => getSwapUserInfo(params, signal),
        placeholderData: keepPreviousData,
        enabled,
    });
};

const getSwapTradeEstimate = (params, signal) =>
    axios.get("/wapi/swap/trade_estimate", { params, signal }).then((response) => response.data);
export const useSwapTradeEstimate = (params, enabled) => {
    return useQuery({
        queryKey: ["swap", "trade_estimate", params, enabled],
        queryFn: ({ signal }) => getSwapTradeEstimate(params, signal),
        enabled,
        staleTime: 0,
        gcTime: 0,
    });
};
