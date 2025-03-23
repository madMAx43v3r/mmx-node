import { useQueryClient } from "@tanstack/vue-query";

class Prefetch {
    constructor() {
        this.queryClient = useQueryClient();
        this.router = useRouter();
    }

    path(path) {
        if (path) {
            const route = this.router.resolve(path);
            if (route.meta?.prefetcher) {
                route.meta.prefetcher(this.queryClient, route);
            }
        }
        return path;
    }
}

export const usePrefetch = () => new Prefetch();
