import axios from "@/queries/axios";
import { useMutation, useQueryClient, keepPreviousData } from "@tanstack/vue-query";
import { useQueryWrapper as useQuery } from "@/composables/useQueryWrapper";
import { ONE_SECOND, onError, pushSuccess, pushError } from "../common";
import i18n from "@/plugins/i18n";
const { t } = i18n.global;

const getOffers = (params, signal) => axios.get("/wapi/offers", { params, signal }).then((response) => response.data);
export const useOffers = (params) => {
    return useQuery({
        queryKey: ["market", "offers", params],
        queryFn: ({ signal }) => getOffers(params, signal),
        placeholderData: keepPreviousData,
    });
};

const getTradeHistory = (params, signal) =>
    axios.get("/wapi/trade_history", { params, signal }).then((response) => response.data);
export const useTradeHistory = (params) => {
    return useQuery({
        queryKey: ["market", "offers", "trade_history", params],
        queryFn: ({ signal }) => getTradeHistory(params, signal),
        placeholderData: keepPreviousData,
    });
};

const getOfferTradeEstimate = (params, signal) =>
    axios.get("/wapi/offer/trade_estimate", { params, signal }).then((response) => response.data);
export const useOffersTradeEstimate = (params, enabled) => {
    return useQuery({
        queryKey: ["market", "offers", "trade_estimate", params, enabled],
        queryFn: ({ signal }) => getOfferTradeEstimate(params, signal),
        enabled,
        staleTime: 0,
        gcTime: 0,
    });
};
