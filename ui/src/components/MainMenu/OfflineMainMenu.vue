<template>
    <MenuTabs :tabs="tabs" />
</template>

<script setup>
import Logo from "./components/Logo/index.vue";
import I18nDropDown from "./components/I18nDropDown.vue";

const { routeTabItem: themeChangerRouteTabItem } = useThemeChanger();

const { t } = useI18n();
const tabs = computed(() => [
    { component: Logo },
    { separator: true },
    { to: "/tx/qr", label: "Offline wallet" },
    { space: true },
    themeChangerRouteTabItem.value,
    { component: I18nDropDown },
]);
</script>
