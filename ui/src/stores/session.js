import { defineStore, getActivePinia, acceptHMRUpdate } from "pinia";
import { useLocalStorage, useSessionStorage } from "@vueuse/core";
import { jsonSerializer } from "@/stores/common";

export const useSessionStore = defineStore("session", {
    state: () => ({
        isLoggedIn: useLocalStorage("isLoggedIn", false),
        credentials: useLocalStorage("credentials", null, {
            serializer: jsonSerializer,
        }),
        autoLogin: useLocalStorage("autoLogin", false),

        activeWalletIndex: useLocalStorage("activeWalletIndex", null, {
            serializer: jsonSerializer,
        }),
        market: {
            menu: {
                bid: null,
                ask: null,
            },
        },
        swap: {
            menu: {
                token: null,
                currency: null,
            },
        },
    }),
    actions: {
        doLogin(credentials) {
            if (this.autoLogin) {
                this.credentials = credentials;
            }

            console.log("login");
            this.isLoggedIn = true;
            if (this.router.currentRoute.value.query?.redirect) {
                this.router.replace(this.router.currentRoute.value.query.redirect);
            } else {
                this.router.replace("/");
            }
        },
        doLogout(userCall) {
            console.log("logout");
            if (userCall) {
                this.credentials = null;
                this.autoLogin = false;
            }

            this.isLoggedIn = false;

            const appStore = useAppStore();
            if (appStore.isGUI) return;

            if (!userCall && this.router.currentRoute.value.path != "/login") {
                this.router.push({
                    path: "/login",
                    // save the location we were at to come back later
                    query: { redirect: this.router.currentRoute.value.fullPath },
                });
            } else {
                this.router.push("/login");
            }

            this.queryClient.clear();
            // reset stores on logout
            getActivePinia()._s.forEach((store) => store.$reset());
        },
    },
});

if (import.meta.hot) {
    import.meta.hot.accept(acceptHMRUpdate(useSessionStore, import.meta.hot));
}
