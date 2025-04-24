<template>
    <q-tabs align="left" indicator-color="primary">
        <q-toolbar class="m-bg-grey">
            <template v-for="(item, key) in tabs" :key="key">
                <q-route-tab
                    :to="item.to"
                    :label="item.label"
                    :icon="item.icon"
                    @click="item.click"
                    @mouseover="item.prefetcher && item.prefetcher(item.to)"
                />
            </template>
        </q-toolbar>
    </q-tabs>
</template>

<script setup>
const prefetch = usePrefetch();
const prefetchFarmers = (event) => {
    prefetch.path("/explore/farmers");
};

const { t } = useI18n();
const tabs = computed(() => [
    { to: "/explore/blocks", label: t("explore_menu.blocks") },
    { to: "/explore/transactions", label: t("explore_menu.transactions") },
    { to: "/explore/farmers", label: t("explore_menu.farmers"), prefetcher: prefetchFarmers },
]);
</script>
