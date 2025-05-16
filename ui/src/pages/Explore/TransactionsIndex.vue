<template>
    <q-card flat>
        <q-table
            :rows="rows"
            :columns="columns"
            :loading="loading"
            :pagination="{ rowsPerPage: 0 }"
            hide-pagination
            flat
        >
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

            <template v-slot:body-cell-id="bcProps">
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
    </q-card>
</template>

<script setup>
const props = defineProps({
    limit: {
        type: Number,
        default: 100,
    },
});

const { t } = useI18n();

const columns = computed(() => [
    { name: "height", label: t("explore_transactions.height"), field: "height", headerStyle: "width: 7%" },
    { label: t("explore_transactions.type"), field: "note", headerStyle: "width: 10%" },
    {
        label: t("explore_transactions.fee"),
        field: (row) => row.fee.value,
        headerStyle: "width: 7%",
        xclasses: "main-currency",
    },
    {
        label: t("explore_transactions.n_in"),
        field: (row) => row.inputs.length,
        headerStyle: "width: 5%",
    },
    {
        label: t("explore_transactions.n_out"),
        field: (row) => row.outputs.length,
        headerStyle: "width: 5%",
    },
    {
        label: t("explore_transactions.n_op"),
        field: (row) => row.operations.length,
        headerStyle: "width: 5%",
    },
    {
        name: "id",
        label: t("explore_transactions.transaction_id"),
        field: "id",
    },
    {
        label: t("account_tx_history.time"),
        field: "time_stamp",
        format: (item) => new Date(item).toLocaleString(),
        align: "left",
    },
]);

import { useTransactions } from "@/queries/wapi";
const { rows, loading } = useTransactions(props);
</script>
