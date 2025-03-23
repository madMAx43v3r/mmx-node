<template>
    <q-select
        v-model="appStore.locale"
        emit-value
        map-options
        :label="$t('node_settings.language')"
        :options="availableLanguages"
    />
    <q-select
        v-model="appStore.isDarkTheme"
        emit-value
        map-options
        :label="$t('node_settings.theme')"
        :options="themes"
    />
</template>

<script setup>
import { availableLanguages } from "@/plugins/i18n";

const { t } = useI18n();
const appStore = useAppStore();

const themes = computed(() => {
    return [
        { value: false, label: t("node_settings.light") },
        { value: true, label: t("node_settings.dark") },
    ];
});
</script>
