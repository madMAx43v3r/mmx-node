export const useSwapMenuStore = () => {
    const sessionStore = useSessionStore();
    const { swap } = storeToRefs(sessionStore);
    const { menu } = toRefs(swap.value);
    const { token, currency } = toRefs(menu.value);

    return { token, currency };
};
