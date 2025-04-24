import { defaultLocale, validateLocale } from "@/plugins/i18n";
import { defineStore, acceptHMRUpdate } from "pinia";
import { useLocalStorage /*, useSessionStorage */ } from "@vueuse/core";

const _isWinGUI = typeof window.mmx !== "undefined";
const _isQtGUI = typeof window.mmx_qtgui !== "undefined";

export const useAppStore = defineStore("app", {
    state: () => ({
        isDarkTheme: useLocalStorage("isDarkTheme", true),
        locale: useLocalStorage("locale", defaultLocale),
        _wapiBaseUrl: useLocalStorage("wapiBaseUrl", null),
    }),
    getters: {
        isWinGUI: () => _isWinGUI,
        isQtGUI: () => _isQtGUI,
        isGUI: () => _isWinGUI || _isQtGUI,
        wapiBaseUrl: (state) => {
            // eslint-disable-next-line no-undef
            if (typeof __ALLOW_CUSTOM_RPC__ !== "undefined" && __ALLOW_CUSTOM_RPC__ === true) {
                if (!isEmpty(state._wapiBaseUrl)) {
                    return state._wapiBaseUrl;
                }
            }

            if (typeof __WAPI_URL__ !== "undefined") {
                // eslint-disable-next-line no-undef
                return __WAPI_URL__;
            }

            return null;
        },
    },
});

if (import.meta.hot) {
    import.meta.hot.accept(acceptHMRUpdate(useAppStore, import.meta.hot));
}
