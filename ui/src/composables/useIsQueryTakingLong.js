import { ref, onUnmounted } from "vue";
import { useQueryClient } from "@tanstack/vue-query";

export function useIsQueryTakingLong(threshold = 10000) {
    const queryClient = useQueryClient();
    const queryCache = queryClient.getQueryCache();
    const isQueryTakingLong = ref(false);

    // Track queries that have exceeded the threshold
    const longQueries = ref(new Set());

    // Store active timeouts for each query
    const timeouts = new Map();

    const unsubscribe = queryCache.subscribe((event) => {
        if (event.type === "updated") {
            const query = event.query;
            const queryKey = JSON.stringify(query.queryKey);

            if (query.state.fetchStatus === "fetching") {
                // Clear existing timeout if query updates while loading
                if (timeouts.has(queryKey)) {
                    clearTimeout(timeouts.get(queryKey));
                    timeouts.delete(queryKey);
                }

                // Set new timeout to check after threshold
                const timeoutId = setTimeout(() => {
                    if (query.state.fetchStatus === "fetching") {
                        longQueries.value.add(queryKey);
                        isQueryTakingLong.value = true;
                    }
                    timeouts.delete(queryKey);
                }, threshold);

                timeouts.set(queryKey, timeoutId);
            } else {
                // Cleanup when query finishes
                if (timeouts.has(queryKey)) {
                    clearTimeout(timeouts.get(queryKey));
                    timeouts.delete(queryKey);
                }

                if (longQueries.value.has(queryKey)) {
                    longQueries.value.delete(queryKey);
                    isQueryTakingLong.value = longQueries.value.size > 0;
                }
            }
        }
    });

    onUnmounted(() => {
        unsubscribe();
        // Cleanup all pending timeouts
        timeouts.forEach((timeout) => clearTimeout(timeout));
        timeouts.clear();
    });

    return isQueryTakingLong;
}
