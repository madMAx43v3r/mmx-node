<template>
    <q-table :rows="rows" :columns="columns" :loading="loading" :pagination="{ rowsPerPage: 0 }" hide-pagination flat>
        <template v-if="loading" v-slot:bottom-row="brProps">
            <TableBodySkeleton :props="brProps" :row-count="props.limit" />
        </template>

        <template v-slot:body-cell-height="bcProps">
            <q-td :props="bcProps">
                <RouterLink :to="`/explore/block/height/${bcProps.value}`" class="text-primary">
                    {{ bcProps.value }}
                </RouterLink>
            </q-td>
        </template>

        <template v-slot:body-cell-address="bcProps">
            <q-td :props="bcProps">
                <RouterLink :to="`/explore/address/${bcProps.value}`" class="text-primary">
                    {{ bcProps.value }}
                </RouterLink>
            </q-td>
        </template>

        <template v-slot:body-cell-tx="bcProps">
            <q-td :props="bcProps">
                <RouterLink :to="`/explore/transaction/${bcProps.value}`" class="text-primary">TX</RouterLink>
            </q-td>
        </template>
    </q-table>
</template>

<script setup>
const props = defineProps({
    address: {
        type: String,
        required: true,
    },
    limit: {
        type: Number,
        default: 200,
        required: false,
    },
});

const { t } = useI18n();
const columns = computed(() => [
    {
        name: "height",
        label: t("common.height"),
        field: "height",
        headerStyle: "width: 7%",
    },
    {
        label: t("common.type"),
        field: "type",
        classes: (item) =>
            new Map([
                ["BUY", "text-green"],
                ["SELL", "text-red"],
            ]).get(item.type) || "",
        headerStyle: "width: 11%",
    },

    {
        label: t("common.amount"),
        field: "value",
        headerStyle: "width: 10%",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
        align: "right",
    },
    {
        // label: t("common.symbol"),
        field: "symbol",
        headerStyle: "width: 5%",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
        align: "left",
    },

    {
        name: "address",
        label: t("common.user"),
        field: "user",
        align: "left",
    },
    {
        name: "tx",
        //label: t("common.link"),
        field: "txid",
        headerStyle: "width: 1%",
    },
    {
        label: t("common.time"),
        field: "time_stamp",
        format: (item) => new Date(item).toLocaleString(),
        headerStyle: "width: 15%",
        align: "left",
    },
]);

import { useSwapHistory } from "@/queries/wapi";
const { rows, loading } = useSwapHistory(reactive({ id: toRef(() => props.address), limit: toRef(() => props.limit) }));
</script>
