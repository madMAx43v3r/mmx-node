import { defineStore, acceptHMRUpdate } from "pinia";

export const useWalletStore = defineStore("wallet", {
    state: () => ({
        wallet: null,
    }),
    actions: {
        doLogout() {
            this.wallet = null;
        },
    },
});

if (import.meta.hot) {
    import.meta.hot.accept(acceptHMRUpdate(useNodeStore, import.meta.hot));
}
