<template>
    <div class="q-gutter-y-sm">
        <SwapListMenu />
        <q-table
            :rows="rows"
            :columns="columns"
            :loading="loading"
            :pagination="{ rowsPerPage: 0 }"
            hide-pagination
            flat
        >
            <template v-if="loading" v-slot:bottom-row="brProps">
                <TableBodySkeleton :props="brProps" :row-count="20" />
            </template>

            <template v-slot:body-cell-symbol="bcProps">
                <q-td :props="bcProps">
                    <template v-if="bcProps.value.symbol == 'MMX'">
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

            <template v-slot:body-cell-actions="bcProps">
                <q-td :props="bcProps" class="q-gutter-x-xs">
                    <q-btn :label="t('swap.swap')" outline :to="`/swap/${bcProps.row.address}`" />
                </q-td>
            </template>
        </q-table>
    </div>
</template>

<script setup>
import SwapListMenu from "./SwapListMenu.vue";

import { useSwapMenuStore } from "../../Swap/useSwapMenuStore";
const { token, currency } = useSwapMenuStore();
const { activeWalletIndex } = useActiveWalletStore();

const { t } = useI18n();
const columns = computed(() => [
    {
        label: t("common.name"),
        field: "name",
        headerStyle: "width: 15%",
    },
    {
        name: "symbol",
        label: t("common.token"),
        field: (row) => {
            return { symbol: row.symbols[0], currency: row.tokens[0] };
        },
        headerStyle: "width: 5%",
    },
    {
        name: "symbol",
        label: t("common.currency"),
        field: (row) => {
            return { symbol: row.symbols[1], currency: row.tokens[1] };
        },
        headerStyle: "width: 5%",
    },

    // price
    {
        label: t("common.price"),
        field: "price",
        format: (item) => (item ? parseFloat(item.toPrecision(6)) : "N/A"),
        headerStyle: "width: 8%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        name: "price-symbol",
        field: (row) => ({ symbol1: row.symbols[1], symbol2: row.symbols[0] }),
        headerStyle: "width: 10%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },

    // pool balance 1
    {
        label: t("swap.pool_balance"),
        field: (row) => row.balance[0].value,
        format: (item) => parseFloat(item.toPrecision(6)),
        headerStyle: "width: 12%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        field: (row) => row.symbols[0],
        headerStyle: "width: 10%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    // pool balance 2
    {
        label: t("swap.pool_balance"),
        field: (row) => row.balance[1].value,
        format: (item) => parseFloat(item.toPrecision(6)),
        headerStyle: "width: 8%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        field: (row) => row.symbols[1],
        headerStyle: "width: 10%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    { name: "actions", align: "right" },
]);

import { useSwapList, useWalletTokens } from "@/queries/wapi";
const { rows: tokens } = useWalletTokens();
const { rows, loading } = useSwapList(reactive({ token, currency, limit: 200 }));
</script>
