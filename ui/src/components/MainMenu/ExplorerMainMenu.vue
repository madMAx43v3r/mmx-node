<template>
    <MenuTabs :tabs="tabs" />
</template>

<script setup>
import Logo from "./components/Logo/index.vue";
import I18nDropDown from "./components/I18nDropDown.vue";

const { routeTabItem: themeChangerRouteTabItem } = useThemeChanger();
// const { routeTabItem: wapiChangerRouteTabItem } = useWapiChanger();

const { t } = useI18n();
const tabs = computed(() => [
    { component: Logo },
    { separator: true },
    { to: "/explore", label: t("main_menu.explore") },
    { to: "/wallet", label: t("main_menu.wallet") },
    { space: true },
    // wapiChangerRouteTabItem.value,
    themeChangerRouteTabItem.value,
    { component: I18nDropDown },
]);

import { useNodeInfo } from "@/queries/wapi";
useNodeInfo({ refreshInterval: 5000 });
</script>
