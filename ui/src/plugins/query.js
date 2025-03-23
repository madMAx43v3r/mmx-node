import { VueQueryPlugin, QueryClient, QueryCache } from "@tanstack/vue-query";

const queryClient = new QueryClient({
    defaultOptions: {
        queries: {
            retry: true,
            networkMode: "always",
        },
        mutations: {
            networkMode: "always",
        },
    },
});

export const vueQueryPluginOptions = {
    queryClient,
};

export default VueQueryPlugin;
