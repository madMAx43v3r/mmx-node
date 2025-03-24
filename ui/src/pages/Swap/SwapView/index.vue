<template>
    <div class="q-gutter-y-sm">
        <div>
            <div class="row q-gutter-x-none">
                <m-chip>{{ $t("swap.swap") }}</m-chip>
                <m-chip v-if="data">
                    {{ data.price ? parseFloat(data.price.toPrecision(6)) : "N/A" }}
                    {{ data.symbols[1] }} / {{ data.symbols[0] }}
                </m-chip>
                <m-chip>{{ address }}</m-chip>
            </div>

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

                <template v-slot:body-cell-symbol="bcProps">
                    <q-td :props="bcProps">
                        <template v-if="bcProps.value.symbol == 'MMX' || bcProps.value.link == false">
                            {{ bcProps.value.symbol }}
                        </template>
                        <template v-else>
                            <RouterLink :to="`/explore/address/${bcProps.value.currency}`" class="text-primary">
                                {{ bcProps.value.symbol }}
                                <span
                                    v-if="!tokens.some((t) => t.currency == bcProps.value.currency)"
                                    class="text-orange mono"
                                    >?</span
                                >
                            </RouterLink>
                        </template>
                    </q-td>
                </template>

                <template v-slot:body-cell-price-symbol="bcProps">
                    <q-td :props="bcProps">
                        <span>{{ bcProps.value.symbol1 }} / {{ bcProps.value.symbol2 }}</span>
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
        </div>
        <div v-if="!noData">
            <SwapViewMenu :address="address" />
            <TrRouterView :address="address" />
        </div>
        <div v-else>
            <EmptyState title="No such swap" :icon="mdiBankTransfer" />
        </div>
    </div>
</template>

<script setup>
import SwapViewMenu from "./SwapViewMenu.vue";
import { mdiBankTransfer } from "@mdi/js";

const props = defineProps({
    address: {
        type: String,
        required: true,
    },
});

const { t } = useI18n();
const columns = computed(() => [
    {
        field: "name",
        headerStyle: "width: 5%",
        classes: "key-cell m-bg-grey",
    },
    {
        label: t("swap.pool_balance"),
        field: "balance",
        format: (item) => parseFloat(item.toPrecision(6)),
        headerStyle: "width: 10%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        name: "symbol",
        field: (row) => ({ symbol: row.symbol, currency: row.token }),
        headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    // volume 24h
    {
        label: `${t("swap.volume")} (${t("swap.24h")})`,
        field: "volume_1d",
        format: (item) => parseFloat(item.toPrecision(6)),
        headerStyle: "width: 10%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        name: "symbol",
        field: (row) => ({ symbol: row.symbol, currency: row.token, link: false }),
        headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    // volume 7d
    {
        label: `${t("swap.volume")} (${t("swap.7d")})`,
        field: "volume_7d",
        format: (item) => parseFloat(item.toPrecision(6)),
        headerStyle: "width: 10%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        name: "symbol",
        field: (row) => ({ symbol: row.symbol, currency: row.token, link: false }),
        headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    //
    {
        label: `${t("swap.apy")} (${t("swap.24h")})`,
        field: "avg_apy_1d",
        format: (item) => (item * 100).toFixed(2),
        headerStyle: "width: 10%",
        align: "right",
        xclasses: "percents",
    },
    {
        label: `${t("swap.apy")} (${t("swap.7d")})`,
        field: "avg_apy_7d",
        format: (item) => (item * 100).toFixed(2),
        headerStyle: "width: 10%",
        align: "right",
        xclasses: "percents",
    },
]);

import { useSwapInfo, useWalletTokens } from "@/queries/wapi";
const { rows: tokens } = useWalletTokens();
const { data, loading, noData } = useSwapInfo(reactive({ id: toRef(() => props.address) }));

const tableData = computed(() => {
    if (!data.value) return [];
    const { balance, symbols, tokens, volume_1d, volume_7d, avg_apy_1d, avg_apy_7d } = data.value;
    const res = [{ name: t("common.token") }, { name: t("common.currency") }];
    res.map((row, i) => {
        Object.assign(row, {
            balance: balance[i].value,
            symbol: symbols[i],
            token: tokens[i],
            volume_1d: volume_1d[i].value,
            volume_7d: volume_7d[i].value,
            avg_apy_1d: avg_apy_1d[i],
            avg_apy_7d: avg_apy_7d[i],
        });
    });
    return res;
});
</script>
