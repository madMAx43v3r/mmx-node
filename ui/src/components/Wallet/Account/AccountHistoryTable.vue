<template>
    <q-table
        :rows="rows"
        :columns="columns"
        :loading="loading"
        :pagination="{ rowsPerPage: 0 }"
        hide-pagination
        wrap-cells
        flat
    >
        <template v-if="loading" v-slot:bottom-row="brProps">
            <TableBodySkeleton :props="brProps" :row-count="6" />
        </template>

        <template v-slot:body-cell-height="bcProps">
            <q-td :props="bcProps">
                <template v-if="bcProps.row.is_pending">
                    <i>pending</i>
                </template>
                <template v-else>
                    <RouterLink :to="`/explore/block/height/${bcProps.value}`" class="text-primary">
                        {{ bcProps.value }}
                    </RouterLink>
                </template>
            </q-td>
        </template>

        <template v-slot:body-cell-token="bcProps">
            <q-td :props="bcProps">
                <template v-if="bcProps.row.is_native">
                    {{ bcProps.value }}
                </template>
                <template v-else>
                    <RouterLink :to="`/explore/address/${bcProps.row.contract}`" class="text-primary mono">
                        {{ bcProps.row.is_nft ? "[NFT]" : bcProps.row.symbol }}
                        <span v-if="!bcProps.row.is_validated" class="text-orange">?</span>
                    </RouterLink>
                </template>
            </q-td>
        </template>

        <template v-slot:body-cell-address="bcProps">
            <q-td :props="bcProps">
                <RouterLink :to="`/explore/address/${bcProps.value}`" class="text-primary mono">
                    {{ getShortAddr(bcProps.value) }}
                </RouterLink>
            </q-td>
        </template>

        <template v-slot:body-cell-txid="bcProps">
            <q-td :props="bcProps">
                <TxTypeLink :id="bcProps.value" :type="bcProps.row.type" class="text-primary">TX</TxTypeLink>
            </q-td>
        </template>

        <template v-if="exportEnabled" v-slot:top-right>
            <q-btn
                color="primary"
                :icon-right="mdiArchiveArrowDown"
                label="Export to csv"
                outline
                rounded
                no-caps
                @click="handleExportTable"
            />
        </template>
    </q-table>
</template>

<script setup>
import { mdiArchiveArrowDown } from "@mdi/js";

const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
    limit: {
        type: Number,
        required: true,
    },
    memo: {
        type: String,
        required: false,
        default: null,
    },
    type: {
        type: String,
        required: false,
        default: null,
    },
    currency: {
        type: String,
        required: false,
        default: null,
    },
    exportEnabled: {
        type: Boolean,
        required: false,
        default: false,
    },
});

const { t } = useI18n();
const columns = computed(() => [
    {
        name: "height",
        label: t("account_history.height"),
        field: "height",
        headerStyle: "width: 7%",
    },
    {
        label: t("account_history.type"),
        field: "type",
        headerStyle: "width: 8%",
        classes: (row) => getTypeFieldCssClasses(row.type),
    },
    {
        label: t("account_history.amount"),
        field: "value",
        headerClasses: "dense-amount",
        classes: "dense-amount",
        headerStyle: "width: 7%",
        style: "font-weight: bold;",
        align: "right",
    },
    {
        name: "token",
        label: t("account_history.token"),
        field: "symbol",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
        headerStyle: "width: 5%",
        align: "left",
    },
    {
        name: "address",
        label: t("account_history.address"),
        field: "address",
        headerStyle: "width: 15%",
        align: "left",
    },
    {
        label: "Memo", // TODO i18n
        field: "memo",
        style: "word-break: break-all;",
        align: "left",
    },
    {
        name: "txid",
        label: t("account_history.link"),
        field: "txid",
        headerStyle: "width: 5%",
    },
    {
        label: t("account_history.time"),
        field: (data) => data,
        format: (data) => (!data.is_pending ? new Date(data.time_stamp).toLocaleString() : ""),
        headerStyle: "width: 15%",
        classes: "text-no-wrap",
        align: "left",
    },
]);

import { useWalletHistory } from "@/queries/wapi";
const { rows, loading } = useWalletHistory(
    reactive({
        index: toRef(() => props.index),
        limit: toRef(() => props.limit),
        memo: toRef(() => props.memo),
        type: toRef(() => props.type),
        currency: toRef(() => props.currency),
    })
);

const { exportTable } = useExportTable();
const handleExportTable = () => {
    exportTable(rows.value, columns.value);
};
</script>
