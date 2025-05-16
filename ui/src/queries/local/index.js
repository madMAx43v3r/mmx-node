import axios from "@/queries/axios";
import { useQuery } from "@tanstack/vue-query";
import { ONE_SECOND, noCacheHeaders } from "../common";

const getGuiBuildInfo = (signal) =>
    axios.get("./guiBuildInfo.json", { signal, headers: noCacheHeaders }).then((response) => response.data);
export const useGuiBuildInfo = () => {
    return useQuery({
        queryKey: ["guiBuildInfo"],
        queryFn: ({ signal }) => getGuiBuildInfo(signal),
        refetchInterval: 5 * ONE_SECOND,
    });
};
