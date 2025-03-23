let interval;

export const useWatchTheme = () => {
    const $q = useQuasar();
    const appStore = useAppStore();
    watchEffect(() => $q.dark.set(appStore.isDarkTheme));

    if (window.mmx?.theme_dark !== undefined && !interval) {
        console.log(window.mmx.theme_dark, interval);
        interval = setInterval(() => {
            appStore.isDarkTheme = window.mmx.theme_dark;
        }, 500);
    }
};
