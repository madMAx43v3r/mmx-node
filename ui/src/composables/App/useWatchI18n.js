import i18n, { loadAndSetI18nLanguage } from "@/plugins/i18n";
import { nextTick } from "vue";

let interval;
export const useWatchI18n = () => {
    const appStore = useAppStore();

    const setLocale = async (locale) => {
        loadAndSetI18nLanguage(i18n, locale);
        return nextTick();
    };
    watchEffect(() => setLocale(appStore.locale));

    if (window.mmx?.locale !== undefined && !interval) {
        interval = setInterval(() => {
            appStore.locale = window.mmx.locale;
        }, 500);
    }
};
