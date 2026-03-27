// Utilities
import router from "@/plugins/router";
import { createPinia } from "pinia";
import { useQueryClient } from "@tanstack/vue-query";

const pinia = createPinia();

pinia.use(({ store }) => {
    store.router = markRaw(router);
    store.queryClient = markRaw(useQueryClient());
});

export default pinia;
