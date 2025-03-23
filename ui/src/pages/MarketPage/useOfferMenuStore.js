import { storeToRefs } from "pinia";

export const useOfferMenuStore = () => {
    const sessionStore = useSessionStore();
    const { market } = storeToRefs(sessionStore);
    const { menu } = toRefs(market.value);
    const { bid, ask } = toRefs(menu.value);

    return { bid, ask };
};
