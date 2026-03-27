<template>
    <q-table :rows="rows" :columns="columns" :loading="loading" :pagination="{ rowsPerPage: 0 }" hide-pagination flat>
        <template v-if="loading" v-slot:bottom-row="brProps">
            <TableBodySkeleton :props="brProps" :row-count="6" />
        </template>

        <template v-slot:body-cell-height="bcProps">
            <q-td :props="bcProps">
                <RouterLink :to="`/explore/block/height/${bcProps.value}`" class="text-primary">
                    {{ bcProps.value }}
                </RouterLink>
            </q-td>
        </template>

        <template v-slot:body-cell-transaction_id="bcProps">
            <q-td :props="bcProps">
                <RouterLink :to="`/explore/transaction/${bcProps.value}`" class="text-primary mono">
                    {{ getShortHash(bcProps.value, 16) }}
                </RouterLink>
            </q-td>
        </template>

        <template v-slot:body-cell="bcProps">
            <q-td :props="bcProps">
                <div :class="getXClasses(bcProps)">
                    {{ bcProps.value }}
                </div>
            </q-td>
        </template>
    </q-table>
</template>
<script setup>
const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
    limit: {
        type: Number,
        required: false,
        default: 200,
    },
});

const { t } = useI18n();
const columns = computed(() => [
    {
        name: "height",
        label: t("account_tx_history.height"),
        field: "height",
        headerStyle: "width: 7%",
    },
    {
        label: "Type", // TODO i18n
        field: "note",
        headerStyle: "width: 11%",
    },
    {
        label: "Fee", // TODO i18n
        field: (row) => row.height && row.fee && row.fee.value,
        format: (item) => (item ? item.toFixed(3) : "..."),
        headerStyle: "width: 12%",
        xclasses: "main-currency",
    },
    {
        label: t("account_tx_history.confirmed"),
        field: "confirm",
        format: (confirm) => (confirm ? (confirm > 1000 ? "> 1000" : confirm) : null),
        headerStyle: "width: 7%",
    },
    {
        label: t("account_tx_history.status"),
        field: "state",
        headerStyle: "width: 7%",
        classes: (row) => (row.state == "failed" ? "text-negative" : null),
    },
    {
        name: "transaction_id",
        label: t("account_tx_history.transaction_id"),
        field: "id",
        align: "left",
    },
    {
        label: t("account_tx_history.time"),
        field: "time",
        format: (item) => new Date(item).toLocaleString(),
        headerStyle: "width: 15%",
        align: "left",
    },
]);

import { useWalletTxHistory } from "@/queries/wapi";
const { rows, loading } = useWalletTxHistory(props);
</script>
