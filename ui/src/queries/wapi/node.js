import axios from "@/queries/axios";
import { useMutation, useQueryClient, keepPreviousData } from "@tanstack/vue-query";
import { useQueryWrapper as useQuery } from "@/composables/useQueryWrapper";
import { ONE_SECOND } from "../common";

var prevHeight = -1;
const getNodeInfo = (signal) => axios.get("/wapi/node/info", { signal }).then((response) => response.data);
export const useNodeInfo = (params) => {
    const query = useQuery({
        queryKey: ["node_info"],
        queryFn: ({ signal }) => getNodeInfo(signal),
        refetchInterval: params?.refreshInterval ?? 1.1 * ONE_SECOND,
        enabled: params?.enabled ?? true,
    });

    const nodeStore = useNodeStore();
    const queryClient = useQueryClient();
    watch(query.data, (data) => {
        if (!data) return;
        const height = data.height;
        if (prevHeight != height) {
            console.log(`⛰️ New height: ${height}`);

            nodeStore.height = height;

            //queryClient.invalidateQueries({ queryKey: ["session"] });
            queryClient.invalidateQueries({ queryKey: ["node"] }, { cancelRefetch: false });
            queryClient.invalidateQueries({ queryKey: ["node_log"] }, { cancelRefetch: false });

            queryClient.invalidateQueries({ queryKey: ["block", { height: prevHeight }] });
            queryClient.invalidateQueries({ queryKey: ["block", { height: height - 1 }] });
            queryClient.invalidateQueries({ queryKey: ["block", { height }] });

            queryClient.invalidateQueries({ queryKey: ["transaction"] }), { cancelRefetch: false };

            queryClient.invalidateQueries({ queryKey: ["wallet"] }), { cancelRefetch: false };
            queryClient.invalidateQueries({ queryKey: ["market"] }), { cancelRefetch: false };
            queryClient.invalidateQueries({ queryKey: ["swap"] }), { cancelRefetch: false };

            queryClient.invalidateQueries({ queryKey: ["farm", "info"] }, { cancelRefetch: false });
            if (height % 3 == 0) {
                queryClient.invalidateQueries({ queryKey: ["farm", "blocks"] }, { cancelRefetch: false });
            }

            prevHeight = height;
        }
    });

    return query;
};

export const useNodeInfoMutation = () => {
    useNodeInfo();
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: () => getNodeInfo(),
        onSuccess: (data) => {
            queryClient.setQueryData(["node_info"], data);
        },
    });
};

const getNodeLog = (params, signal) =>
    axios.get("/wapi/node/log", { params, signal }).then((response) => response.data);
export const useNodeLog = (params) => {
    return useQuery({
        queryKey: ["node_log", params],
        queryFn: ({ signal }) => getNodeLog(params, signal),
        placeholderData: keepPreviousData,
        refetchInterval: 2 * ONE_SECOND,
    });
};

const getGraphBlocks = (params, signal) =>
    axios.get("/wapi/node/graph/blocks", { params, signal }).then((response) => response.data);
export const useGraphBlocks = (params, select) => {
    return useQuery({
        queryKey: ["node", "graph/blocks", params],
        queryFn: ({ signal }) => getGraphBlocks(params, signal),
        select,
    });
};

export const prefetchGraphBlocks = (queryClient, params) => {
    queryClient.prefetchQuery({
        queryKey: ["node", "graph/blocks", params],
        queryFn: ({ signal }) => getGraphBlocks(params, signal),
    });
};
