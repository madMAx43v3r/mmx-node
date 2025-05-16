export const authMiddleware = async (to, from) => {
    const appStore = useAppStore();
    const sessionStore = useSessionStore();
    const requiresAuth = to.matched.some((x) => x.meta.requiresAuth || !Object.hasOwn(x.meta, "requiresAuth"));

    if (requiresAuth && !sessionStore.isLoggedIn && !appStore.isGUI) {
        return {
            path: "/login",
            // save the location we were at to come back later
            query: { redirect: to.fullPath },
        };
    }

    if (to.name === "login" && sessionStore.isLoggedIn && !appStore.isGUI) {
        return {
            path: "/",
        };
    }
};
