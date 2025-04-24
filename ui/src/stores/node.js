import { defineStore, acceptHMRUpdate } from "pinia";
export const useNodeStore = defineStore("node", {
    state: () => ({
        height: null,
    }),
});

if (import.meta.hot) {
    import.meta.hot.accept(acceptHMRUpdate(useNodeStore, import.meta.hot));
}
