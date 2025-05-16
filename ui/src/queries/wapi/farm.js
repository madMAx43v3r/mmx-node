import axios from "@/queries/axios";
import { useQueryWrapper as useQuery } from "@/composables/useQueryWrapper";

const getFarmInfo = (signal) => axios.get("/wapi/farm/info", { signal }).then((response) => response.data);
export const useFarmInfo = (select) => {
    return useQuery({
        queryKey: ["farm", "info"],
        queryFn: ({ signal }) => getFarmInfo(signal),
        select: (data) => (select ? select(data) : data),
    });
};

const getFarmBlocksSummary = (params, signal) =>
    axios.get("/wapi/farm/blocks/summary", { params, signal }).then((response) => response.data);
export const useFarmBlocksSummary = (params) => {
    return useQuery({
        queryKey: ["farm", "blocks", "summary", params],
        queryFn: ({ signal }) => getFarmBlocksSummary(params, signal),
    });
};

const getFarmBlocks = (params, signal) =>
    axios.get("/wapi/farm/blocks", { params, signal }).then((response) => response.data);
export const useFarmBlocks = (params) => {
    return useQuery({
        queryKey: ["farm", "blocks", params],
        queryFn: ({ signal }) => getFarmBlocks(params, signal),
    });
};

const getFarmProofs = (params, signal) =>
    axios.get("/wapi/farm/proofs", { params, signal }).then((response) => response.data);
export const useFarmProofs = (params) => {
    return useQuery({
        queryKey: ["farm", "proofs", params],
        queryFn: ({ signal }) => getFarmProofs(params, signal),
    });
};
