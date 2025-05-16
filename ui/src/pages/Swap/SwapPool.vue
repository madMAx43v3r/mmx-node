<template>
    <div class="q-gutter-y-sm">
        <q-table
            :rows="tableData"
            :columns="columns"
            :loading="loading"
            :pagination="{ rowsPerPage: 0 }"
            hide-pagination
            flat
        >
            <template v-if="loading" v-slot:bottom-row="brProps">
                <TableBodySkeleton :props="brProps" :row-count="4" />
            </template>

            <template v-slot:body-cell="bcProps">
                <q-td :props="bcProps">
                    <div :class="getXClasses(bcProps)">
                        {{ bcProps.value }}
                    </div>
                </q-td>
            </template>
        </q-table>
    </div>
</template>

<script setup>
const props = defineProps({
    address: {
        type: String,
        required: true,
    },
});

const { t } = useI18n();
const columns = computed(() => [
    {
        label: t("swap.fee_level"),
        field: "fee_rate",
        format: (item) => item * 100,
        headerStyle: "width: 5%",
        classes: "key-cell m-bg-grey",
        xclasses: "percents",
    },
    //
    {
        label: t("common.balance"),
        field: "balance0",
        format: (item) => parseFloat(item.toPrecision(6)),
        headerStyle: "width: 8%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        field: "symbol0",
        headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    //
    {
        label: t("common.balance"),
        field: "balance1",
        format: (item) => parseFloat(item.toPrecision(6)),
        headerStyle: "width: 8%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        field: "symbol1",
        headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    // price
    {
        label: "Price", // TODO i18n
        field: "price",
        format: (item) => (item != null ? parseFloat(item.toPrecision(6)) : "N/A"),
        //align: "left",
        headerStyle: "width: 8%",
        classes: "text-bold",
    },
    //
    {
        label: t("swap.user_total"),
        field: "user_total0",
        format: (item) => parseFloat(item.toPrecision(6)),
        headerStyle: "width: 8%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        field: "symbol0",
        headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    //
    {
        label: t("swap.user_total"),
        field: "user_total1",
        format: (item) => parseFloat(item.toPrecision(6)),
        headerStyle: "width: 8%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        field: "symbol1",
        //headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
]);

import { useSwapInfo } from "@/queries/wapi";
const { data, loading } = useSwapInfo(reactive({ id: toRef(() => props.address) }));

const tableData = computed(() => {
    if (!data.value) return [];
    const { fee_rates, pools, symbols } = data.value;
    const res = fee_rates.map((fee_rate, k) => ({
        fee_rate,
        balance0: pools[k].balance[0].value,
        symbol0: data.value.symbols[0],
        balance1: pools[k].balance[1].value,
        symbol1: data.value.symbols[1],
        price: pools[k].price,
        user_total0: pools[k].user_total[0].value,
        user_total1: pools[k].user_total[1].value,
    }));
    return res;
});
</script>
