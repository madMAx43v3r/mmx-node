import axios from "@/queries/axios";
import { useQuery } from "@tanstack/vue-query";
import { ONE_SECOND } from "../common";

const getGithubTags = (signal) =>
    axios.get("https://api.github.com/repos/madMAx43v3r/mmx-node/tags", { signal }).then((response) => response.data);

export const useGithubTags = (select) => {
    return useQuery({
        queryKey: ["externalBuildInfo"],
        queryFn: ({ signal }) => getGithubTags(signal),
        refetchInterval: 60 * 60 * ONE_SECOND,
        staleTime: Infinity,
        select,
    });
};

export const useGithubLatestRelease = () => {
    const select = (data) => {
        for (let item of data) {
            if (item && item.name.length && item.name[0] === "v") {
                return item;
            }
        }
    };
    return useGithubTags(select);
};
