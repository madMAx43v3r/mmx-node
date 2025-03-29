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
                <TableBodySkeleton :props="brProps" :row-count="2" />
            </template>

            <template v-slot:body-cell-fee_level="bcProps">
                <q-td :props="bcProps">
                    <div v-if="bcProps.row.pool_idx >= 0" :class="getXClasses(bcProps)">
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
    //
    {
        label: t("swap.my_balance"),
        field: "balance",
        format: (item) => parseFloat(item.toPrecision(6)),
        headerStyle: "width: 8%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        field: "symbol",
        headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    //
    {
        label: t("swap.my_liquidity"),
        field: "equivalent_liquidity",
        format: (item) => parseFloat(item.toPrecision(6)),
        headerStyle: "width: 8%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        field: "symbol",
        headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    //
    {
        label: t("swap.fees_earned"),
        field: "fees_earned",
        format: (item) => parseFloat(item.toPrecision(6)),
        headerStyle: "width: 8%",
        align: "right",
        headerClasses: "dense-amount",
        classes: (row) => (row.fees_earned > 0 ? "text-positive" : "") + " dense-amount text-bold",
    },
    {
        field: "symbol",
        headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    //
    {
        name: "fee_level",
        label: t("swap.fee_level"),
        field: "fee_level",
        format: (item) => (item * 100).toFixed(2),
        headerStyle: "width: 8%",
        align: "right",
        xclasses: "percents",
    },

    //
    {
        label: t("swap.unlock_height"),
        field: "unlock_height",
        align: "left",
    },
]);

const { activeWalletIndex } = useActiveWalletStore();

import { useWalletAddress } from "@/queries/wapi";
const { data: userAddress } = useWalletAddress(reactive({ index: activeWalletIndex }));

import { useSwapUserInfo } from "@/queries/wapi";
const { data, loading } = useSwapUserInfo(
    reactive({ id: toRef(() => props.address), user: userAddress }),
    () => !!userAddress.value
);

const tableData = computed(() => {
    if (!data.value) return [];
    const { balance, symbols, equivalent_liquidity, fees_earned, pool_idx, unlock_height, swap } = data.value;

    const res = balance.map((row, i) => ({
        balance: balance[i].value,
        equivalent_liquidity: equivalent_liquidity[i].value,
        fees_earned: fees_earned[i].value,
        symbol: symbols[i],
        fee_level: pool_idx >= 0 ? swap.fee_rates[pool_idx] : undefined,
        unlock_height,
        pool_idx,
    }));
    return res;
});
</script>
