import axios from "@/queries/axios";
import { keepPreviousData } from "@tanstack/vue-query";
import { useQueryWrapper as useQuery } from "@/composables/useQueryWrapper";
import { useMutation, useQueryClient } from "@tanstack/vue-query";
import { ONE_SECOND } from "../common";

const getHeaders = (params, signal) => axios.get("/wapi/headers", { params, signal }).then((response) => response.data);
export const useHeaders = (params) => {
    return useQuery({
        queryKey: ["node", "headers", params],
        queryFn: ({ signal }) => getHeaders(params, signal),
    });
};

const getBlock = (params, signal) => axios.get("/wapi/block", { params, signal }).then((response) => response.data);
export const useBlock = (params) => {
    return useQuery({
        queryKey: ["block", params],
        queryFn: ({ signal }) => getBlock(params, signal),
        placeholderData: keepPreviousData,
    });
};

const getHeader = (params, signal) => axios.get("/wapi/header", { params, signal }).then((response) => response.data);
export const useHeader = (params, enabled) => {
    return useQuery({
        queryKey: ["header", params],
        queryFn: ({ signal }) => getHeader(params, signal),
        enabled,
    });
};

export const useCheckHeader = () => {
    return useMutation({
        mutationFn: (payload) => getHeader(payload),
    });
};

export const prefetchBlock = (queryClient, params) => {
    queryClient.prefetchQuery({
        queryKey: ["block", params],
        queryFn: ({ signal }) => getBlock(params, signal),
    });
};

const getTransactions = (params, signal) =>
    axios.get("/wapi/transactions", { params, signal }).then((response) => response.data);
export const useTransactions = (params) => {
    return useQuery({
        queryKey: ["node", "transactions", params],
        queryFn: ({ signal }) => getTransactions(params, signal),
    });
};

const getTransaction = (params, signal) =>
    axios.get("/wapi/transaction", { params, signal }).then((response) => response.data);
export const useTransaction = (params) => {
    return useQuery({
        queryKey: ["transaction", params],
        queryFn: ({ signal }) => getTransaction(params, signal),
        select: (data) => {
            for (const op of data.operations) {
                delete op.solution;
            }
            return data;
        },
    });
};

const getFarmers = (params, signal) => axios.get("/wapi/farmers", { params, signal }).then((response) => response.data);
export const useFarmers = (params) => {
    return useQuery({
        queryKey: ["node", "farmers", params],
        queryFn: ({ signal }) => getFarmers(params, signal),
    });
};

export const prefetchFarmers = (queryClient, params) => {
    queryClient.prefetchQuery({
        queryKey: ["node", "farmers", params],
        queryFn: ({ signal }) => getFarmers(params, signal),
        staleTime: ONE_SECOND,
    });
};

const getFarmer = (params, signal) => axios.get("/wapi/farmer", { params, signal }).then((response) => response.data);
export const useFarmer = (params) => {
    return useQuery({
        queryKey: ["node", "farmer", params],
        queryFn: ({ signal }) => getFarmer(params, signal),
    });
};

const getFarmerBlocks = (params, signal) =>
    axios.get("/wapi/farmer/blocks", { params, signal }).then((response) => response.data);
export const useFarmerBlocks = (params) => {
    return useQuery({
        queryKey: ["node", "farmer", "blocks", params],
        queryFn: ({ signal }) => getFarmerBlocks(params, signal),
    });
};

const getContract = (params, signal) =>
    axios.get("/wapi/contract", { params, signal }).then((response) => response.data);
export const useContract = (params) => {
    return useQuery({
        queryKey: ["node", "contract", params],
        queryFn: ({ signal }) => getContract(params, signal),
    });
};

const getAddressHistory = (params, signal) =>
    axios.get("/wapi/address/history", { params, signal }).then((response) => response.data);
export const useAddressHistory = (params) => {
    return useQuery({
        queryKey: ["node", "address", "history", params],
        queryFn: ({ signal }) => getAddressHistory(params, signal),
    });
};

const getAddress = (params, signal) => axios.get("/wapi/address", { params, signal }).then((response) => response.data);
export const useAddress = (params, select) => {
    return useQuery({
        queryKey: ["node", "address", params],
        queryFn: ({ signal }) => getAddress(params, signal),
        select: (data) => (select ? select(data) : data),
    });
};

const getBalance = (params, signal) => axios.get("/wapi/balance", { params, signal }).then((response) => response.data);
export const useBalance = (params) => {
    return useQuery({
        queryKey: ["node", "balance", params],
        queryFn: ({ signal }) => getBalance(params, signal),
        select: (data) => data.balances,
    });
};
