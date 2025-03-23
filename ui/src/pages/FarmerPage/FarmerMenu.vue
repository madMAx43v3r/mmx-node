<template>
    <q-tabs indicator-color="primary" align="left" class="m-bg-grey">
        <template v-for="(item, key) in tabs" :key="key">
            <q-route-tab :to="item.to" :label="item.label" />
        </template>
    </q-tabs>
</template>

<script setup>
const { t } = useI18n();
const tabs = computed(() => [
    { to: "/farmer/plots", label: t("farmer_menu.plots") },
    { to: "/farmer/plotnfts", label: "NFTs" },
    { to: "/farmer/blocks", label: t("farmer_menu.blocks") },
    { to: "/farmer/proofs", label: t("farmer_menu.proofs") },
]);
</script>
