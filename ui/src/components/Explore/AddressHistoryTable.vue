<template>
    <q-table
        v-if="loading || (!loading && (rows?.length || showEmpty))"
        :rows="rows"
        :columns="columns"
        :loading="loading"
        :pagination="{ rowsPerPage: 0 }"
        hide-pagination
        wrap-cells
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

        <template v-slot:body-cell-token="bcProps">
            <q-td :props="bcProps">
                <template v-if="bcProps.row.is_native">
                    {{ bcProps.row.is_nft ? "[NFT]" : bcProps.value }}
                </template>
                <RouterLink v-else :to="`/explore/address/${bcProps.row.contract}`" class="text-primary mono">
                    {{ bcProps.value }}
                </RouterLink>
            </q-td>
        </template>

        <template v-slot:body-cell-txid="bcProps">
            <q-td :props="bcProps">
                <TxTypeLink :id="bcProps.value" :type="bcProps.row.type" class="text-primary mono">
                    {{ getShortHash(bcProps.value) }}
                </TxTypeLink>
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
        default: 100,
        required: false,
    },
    showEmpty: {
        type: Boolean,
        default: false,
        required: false,
    },
});

const { t } = useI18n();
const columns = computed(() => {
    return [
        { name: "height", label: t("address_history_table.height"), field: "height", headerStyle: "width: 7%" },
        {
            label: t("address_history_table.type"),
            field: "type",
            headerStyle: "width: 7%",
            classes: (row) => getTypeFieldCssClasses(row.type),
        },
        {
            label: t("address_history_table.amount"),
            field: (amount) => amount?.value,
            style: "font-weight: bold;",
            headerStyle: "width: 7%",
        },
        {
            name: "token",
            label: t("address_history_table.token"),
            field: "symbol",
            headerStyle: "width: 7%",
            align: "left",
        },
        {
            name: "txid",
            label: "Transaction ID", // TODO i18n
            field: "txid",
            headerStyle: "width: 20%",
        },
        {
            label: "Memo", // TODO i18n
            field: "memo",
            style: "word-break: break-all",
            align: "left",
        },
        {
            label: t("address_history_table.time"),
            field: "time_stamp",
            format: (item) => new Date(item).toLocaleString(),
            headerStyle: "width: 12%",
            classes: "text-no-wrap",
            align: "left",
        },
    ];
});

import { useAddressHistory } from "@/queries/wapi";
const { rows, loading } = useAddressHistory(
    reactive({ limit: toRef(() => props.limit), id: toRef(() => props.address) })
);
</script>
