<template>
    <q-table :rows="rows" :columns="columns" :loading="loading" :pagination="{ rowsPerPage: 0 }" hide-pagination flat>
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
                        <span v-if="!tokens.some((t) => t.currency == bcProps.value.currency)" class="text-orange mono"
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

        <template v-slot:body-cell-offer="bcProps">
            <q-td :props="bcProps">
                <RouterLink :to="`/explore/address/${bcProps.value}`" class="text-primary">Offer</RouterLink>
            </q-td>
        </template>
        <template v-slot:body-cell-txid="bcProps">
            <q-td :props="bcProps">
                <RouterLink :to="`/explore/transaction/${bcProps.value}`" class="text-primary">TX</RouterLink>
            </q-td>
        </template>
    </q-table>
</template>

<script setup>
import { mdiBankCheck, mdiBankTransfer } from "@mdi/js";

const { t } = useI18n();

const columns = computed(() => [
    // bid
    {
        label: t("market_offers.they_offer"),
        field: "bid_value",
        headerStyle: "width: 8%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        name: "symbol",
        field: (row) => ({ symbol: row.bid_symbol, currency: row.bid_currency }),
        headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    // ask
    {
        label: t("market_offers.they_ask"),
        field: "ask_value",
        headerStyle: "width: 8%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        name: "symbol",
        field: (row) => ({ symbol: row.ask_symbol, currency: row.ask_currency }),
        headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    // price 2
    {
        label: t("market_offers.price"),
        field: (row) => 1 / row.display_price,
        format: (item) => parseFloat(item.toPrecision(3)),
        headerStyle: "width: 8%",
        align: "right",
        headerClasses: "dense-amount",
        classes: "dense-amount text-bold",
    },
    {
        name: "price-symbol",
        field: (row) => ({ symbol1: row.bid_symbol, symbol2: row.ask_symbol }),
        headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    // ----------
    {
        label: t("market_offers.time"),
        field: "time_stamp",
        format: (item) => new Date(item).toLocaleString(),
        headerStyle: "width: 4%",
        align: "left",
    },
    {
        name: "offer",
        field: "address",
        headerStyle: "width: 3%",
    },
    {
        name: "txid",
        field: "txid",
        //headerStyle: "width: 1%",
        align: "left",
    },
]);

import { useOfferMenuStore } from "./useOfferMenuStore";
const { bid, ask } = useOfferMenuStore();

import { useTradeHistory, useWalletTokens } from "@/queries/wapi";
const { rows: tokens } = useWalletTokens();
const { rows, loading } = useTradeHistory(
    reactive({
        bid: toRef(() => bid.value?.value),
        ask: toRef(() => ask.value?.value),
        limit: 200,
    })
);
</script>
