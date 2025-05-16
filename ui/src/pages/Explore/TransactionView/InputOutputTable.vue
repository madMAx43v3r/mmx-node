<template>
    <q-table
        v-if="rows.length > 0"
        :rows="rows"
        :columns="in_out_columns"
        :pagination="{ rowsPerPage: 0 }"
        hide-pagination
        flat
    >
        <template v-if="loading" v-slot:bottom-row="brProps">
            <TableBodySkeleton :props="brProps" :row-count="1" />
        </template>

        <template v-slot:body-cell-index="bcProps">
            <q-td :props="bcProps"> {{ indexPrefix }}[{{ bcProps.rowIndex }}] </q-td>
        </template>

        <template v-slot:body-cell-token="bcProps">
            <q-td :props="bcProps">
                <template v-if="bcProps.row.is_native">{{ bcProps.value }}</template>
                <RouterLink v-else :to="`/explore/address/${bcProps.row.contract}`" class="text-primary">
                    {{ bcProps.value }}
                </RouterLink>
            </q-td>
        </template>

        <template v-slot:body-cell-address="bcProps">
            <q-td :props="bcProps">
                <RouterLink :to="`/explore/address/${bcProps.value}`" class="text-primary mono">
                    {{ bcProps.value }}
                </RouterLink>
            </q-td>
        </template>
    </q-table>
</template>

<script setup>
const props = defineProps({
    rows: {
        type: Object,
        required: true,
    },
    isInput: {
        type: Boolean,
        required: false,
        default: true,
    },
    loading: {
        type: Boolean,
        required: false,
        default: false,
    },
});

const { t } = useI18n();

const indexPrefix = props.isInput ? t("transaction_view.input") : t("transaction_view.output");

const in_out_columns = computed(() => [
    {
        name: "index",
        headerStyle: "width: 10%",
        headerClasses: "key-cell",
        classes: "key-cell m-bg-grey",
    },
    {
        label: t("transaction_view.amount"),
        field: (row) => row.value,
        style: "font-weight: bold;",
        headerStyle: "width: 7%",
    },
    {
        name: "token",
        label: t("transaction_view.token"),
        field: (row) => row.symbol,
        headerStyle: "width: 7%",
    },
    {
        name: "address",
        label: t("transaction_view.address"),
        field: (row) => row.address,
        align: "left",
    },
    {
        label: "Memo", //TODO i18n
        field: (row) => row.memo,
        headerStyle: "width: 50%",
        align: "left",
    },
]);
</script>
