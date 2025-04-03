<template>
    <div>
        <div class="row q-col-gutter-xs">
            <template v-for="item in tiles" :key="item.field">
                <div class="col-md-3 col-sm-6 col-xs-12 col-12">
                    <InfoTile :tile="item" :rows="rows" :loading="loading" />
                </div>
            </template>
        </div>
    </div>
</template>

<script setup>
import { useNodeInfo } from "@/queries/wapi";
const { rows, loading } = useNodeInfo();

const { t } = useI18n();
const tiles = computed(() => [
    {
        field: "is_synced",
        label: t("node_info.synced"),
        format: (item) => (item ? t("common.yes") : t("common.no")),
    },
    {
        field: "height",
        label: t("node_info.height"),
    },
    {
        field: "total_space",
        label: t("node_info.netspace"),
        format: (item) => `${(item / Math.pow(1000, 2)).toFixed(3)} PB`,
    },
    {
        field: "vdf_speed",
        label: t("node_info.vdf_speed"),
        format: (item) => `${item.toFixed(3)} MH/s`,
    },
    {
        field: "address_count",
        label: t("node_info.no_addresses"),
    },
    {
        field: "block_size",
        label: t("node_info.block_size"),
    },
    {
        field: "average_txfee",
        label: "Avg. TX Fee", //TODO i18n
        format: (item) => (item.value > 1 ? item.value.toPrecision(6) : item.value),
    },
    {
        field: "block_reward",
        label: t("node_info.block_reward"),
        format: (item) => `${item.value.toFixed(3)} MMX`,
    },
]);
</script>
