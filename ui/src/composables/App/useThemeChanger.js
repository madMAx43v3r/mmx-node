import { mdiWhiteBalanceSunny, mdiMoonWaxingCrescent } from "@mdi/js";

export const useThemeChanger = () => {
    const appStore = useAppStore();

    const icon = computed(() => (appStore.isDarkTheme ? mdiMoonWaxingCrescent : mdiWhiteBalanceSunny));

    const handleThemeChange = () => {
        appStore.isDarkTheme = !appStore.isDarkTheme;
    };

    const routeTabItem = computed(() => ({
        icon: icon.value,
        click: handleThemeChange,
    }));

    return { routeTabItem, icon, handleThemeChange };
};
