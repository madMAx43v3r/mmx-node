<template>
    <div>
        <q-card v-if="bid?.value" class="q-my-xs" flat>
            <q-card-section>
                <div class="row">
                    <q-input
                        v-model.number="minBidAmount"
                        :debounce="200"
                        :suffix="bid.label"
                        label="Minimum Offer"
                        :rules="[rules.amount]"
                        input-class="amount-input"
                        :clearable="minBidAmount != null"
                        class="col-2"
                    />
                </div>
            </q-card-section>
        </q-card>
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

            <template v-slot:body-cell-address="bcProps">
                <q-td :props="bcProps">
                    <RouterLink :to="`/explore/address/${bcProps.value}`" class="text-primary">TX</RouterLink>
                </q-td>
            </template>

            <template v-slot:body-cell-actions="bcProps">
                <q-td :props="bcProps" class="q-gutter-x-xs">
                    <q-btn
                        :label="t('market_offers.trade')"
                        :_icon="mdiBankTransfer"
                        outline
                        @click="handleTrade(bcProps.row)"
                    />

                    <q-btn
                        :label="t('common.accept')"
                        :_icon="mdiBankCheck"
                        color="positive"
                        outline
                        :disable="accepted.has(bcProps.row.address)"
                        @click="handleAccept(bcProps.row)"
                    />
                </q-td>
            </template>
        </q-table>
    </div>
</template>

<script setup>
import { mdiBankCheck, mdiBankTransfer } from "@mdi/js";
import rules from "@/helpers/rules";

const minBidAmount = ref(null);

const { t } = useI18n();
const columns = computed(() => [
    // bid
    {
        label: t("market_offers.they_offer"),
        field: "bid_balance_value",
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
        field: (row) => row.bid_balance_value * row.display_price,
        format: (item) => parseFloat(item.toPrecision(6)),
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
    //
    {
        label: t("market_offers.time"),
        field: "time_stamp",
        format: (item) => new Date(item).toLocaleString(),
        headerStyle: "width: 12%",
        align: "left",
    },
    {
        name: "address",
        field: "address",
        headerStyle: "width: 1%",
    },
    { name: "actions", align: "right" },
]);

import { useOfferMenuStore } from "./useOfferMenuStore";
const { bid, ask } = useOfferMenuStore();
const { activeWalletIndex } = useActiveWalletStore();

import { useOffers, useWalletTokens } from "@/queries/wapi";

const { rows: tokens } = useWalletTokens();
const { rows, loading } = useOffers(
    reactive({
        bid: toRef(() => bid.value?.value),
        ask: toRef(() => ask.value?.value),
        min_bid: computed(() =>
            bid.value?.value && minBidAmount.value ? minBidAmount.value * Math.pow(10, bid.value.decimals) : null
        ),
        limit: 200,
    })
);

const $q = useQuasar();
const handleTrade = (offer) => {
    $q.dialog({
        component: defineAsyncComponent(() => import("@/components/Dialogs/OfferTradeDialog.vue")),
        componentProps: {
            index: activeWalletIndex.value,
            offer: offer,
        },
    });
};

const accepted = ref(new Set());
const handleAccept = (offer) => {
    $q.dialog({
        component: defineAsyncComponent(() => import("@/components/Dialogs/OfferAcceptDialog.vue")),
        componentProps: {
            index: activeWalletIndex.value,
            offer: offer,
        },
    }).onOk(() => {
        accepted.value.add(offer.address);
    });
};
</script>

<style scoped>
/* ::v-deep(td) {
    padding: 8px;
} */
</style>
