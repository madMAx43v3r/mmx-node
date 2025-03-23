import { storeToRefs } from "pinia";

export const useActiveWalletStore = () => {
    const sessionStore = useSessionStore();
    const { activeWalletIndex } = storeToRefs(sessionStore);

    return { activeWalletIndex };
};
