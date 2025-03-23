<template>
    <q-tabs indicator-color="primary" align="left" class="m-bg-grey">
        <template v-for="(item, key) in tabs" :key="key">
            <q-route-tab :to="item.to" :label="item.label" />
        </template>
    </q-tabs>
</template>

<script setup>
const props = defineProps({
    address: {
        type: String,
        required: true,
    },
});

const { t } = useI18n();
const tabs = computed(() => [
    { to: `/swap/${props.address}/trade`, label: t("swap.trade") },
    { to: `/swap/${props.address}/history`, label: t("swap.history") },
    { to: `/swap/${props.address}/liquid`, label: t("swap.my_liquidity") },
    { to: `/swap/${props.address}/pool`, label: t("swap.pool_state") },
]);
</script>
