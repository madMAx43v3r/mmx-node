<template>
    <q-btn-dropdown auto-close stretch flat :icon="mdiTranslate">
        <q-list>
            <template v-for="(item, key) in availableLanguages" :key="key">
                <q-item clickable @click="handleLanguageChange(item.value)">
                    <q-item-section>
                        {{ item.label }}
                    </q-item-section>
                </q-item>
            </template>
        </q-list>
    </q-btn-dropdown>
</template>

<script setup>
import { mdiTranslate } from "@mdi/js";
import { availableLanguages } from "@/plugins/i18n";

const appStore = useAppStore();
const handleLanguageChange = (locale) => {
    appStore.locale = locale;
};
</script>
