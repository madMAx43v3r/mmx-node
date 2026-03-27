<template>
    <q-card flat>
        <q-card-section>
            <q-select
                v-model="state"
                :options="stateOptions"
                emit-value
                map-options
                :label="$t('account_offers.status')"
            />
        </q-card-section>
    </q-card>
    <q-table :rows="rows" :columns="columns" :loading="loading" :pagination="{ rowsPerPage: 0 }" hide-pagination flat>
        <template v-if="loading" v-slot:bottom-row="brProps">
            <TableBodySkeleton :props="brProps" :row-count="3" />
        </template>

        <template v-slot:body-cell-price-symbol="bcProps">
            <q-td :props="bcProps">
                <span>{{ bcProps.value.symbol1 }} / {{ bcProps.value.symbol2 }}</span>
            </q-td>
        </template>

        <template v-slot:body-cell-contract="bcProps">
            <q-td :props="bcProps">
                <RouterLink :to="`/explore/address/${bcProps.value}`" class="text-primary">TX</RouterLink>
            </q-td>
        </template>

        <template v-slot:body-cell-actions="bcProps">
            <q-td :props="bcProps">
                <div class="row justify-end q-gutter-x-xs q-pa-none">
                    <q-btn
                        :label="t('account_offers.deposit')"
                        :_icon="mdiBankTransferIn"
                        color="positive"
                        outline
                        @click="handleDeposit(bcProps.row)"
                    />

                    <q-btn
                        :disable="!(bcProps.row.ask_balance > 0) || withdrawn.has(bcProps.row.address)"
                        :label="t('account_contract_summary.withdraw')"
                        :_icon="mdiBankTransferOut"
                        color="negative"
                        outline
                        @click="handleWithdraw(bcProps.row)"
                    />

                    <q-separator vertical inset />

                    <q-btn
                        label="Update"
                        :_icon="mdiBankTransferIn"
                        color="primary"
                        outline
                        @click="handleUpdate(bcProps.row)"
                    />

                    <q-btn
                        :disable="!(bcProps.row.bid_balance > 0) || canceled.has(bcProps.row.address)"
                        :label="t('account_offers.revoke')"
                        :_icon="mdiBankRemove"
                        color="secondary"
                        outline
                        @click="handleRevoke(bcProps.row)"
                    />
                </div>
            </q-td>
        </template>
    </q-table>
</template>
<script setup>
import { mdiBankTransferIn, mdiBankTransferOut, mdiBankRemove } from "@mdi/js";
const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

const { t } = useI18n();

const state = ref(true);
const stateOptions = [
    {
        label: t("account_offers.any"),
        value: false,
    },
    {
        label: t("account_offers.open"),
        value: true,
    },
];

const columns = computed(() => [
    {
        label: t("account_offers.offering"),
        field: "bid_balance_value",
        headerStyle: "width: 7%",
        headerClasses: "dense-amount",
        classes: "dense-amount",
        align: "right",
    },
    {
        field: "bid_symbol",
        headerStyle: "width: 5%",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
        align: "left",
    },
    {
        label: t("account_offers.received"),
        field: "ask_balance_value",
        headerStyle: "width: 7%",
        headerClasses: "dense-amount",
        classes: (row) => (row.ask_balance_value > 0 ? "text-positive" : "") + " dense-amount",
        align: "right",
    },
    {
        field: "ask_symbol",
        headerStyle: "width: 5%",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
        align: "left",
    },
    {
        label: t("account_offers.price"),
        field: "display_price",
        format: (item) => parseFloat((1 / item).toPrecision(6)),
        headerStyle: "width: 5%",
        headerClasses: "dense-amount",
        classes: "dense-amount",
        align: "right",
    },
    {
        name: "price-symbol",
        field: (row) => ({ symbol1: row.bid_symbol, symbol2: row.ask_symbol }),
        headerStyle: "width: 5%",
        align: "left",
        headerClasses: "dense-symbol",
        classes: "dense-symbol",
    },
    {
        name: "contract",
        field: "address",
        headerStyle: "width: 5%",
    },
    {
        name: "actions",
    },
]);

import { useWalletOffers } from "@/queries/wapi";
const { rows, loading } = useWalletOffers(reactive({ index: toRef(() => props.index), state }));

const canceled = ref(new Set());
const withdrawn = ref(new Set());

const $q = useQuasar();
const handleDeposit = (offer) => {
    $q.dialog({
        component: defineAsyncComponent(() => import("@/components/Dialogs/OfferDepositDialog")),
        componentProps: {
            index: props.index,
            offer,
        },
    }).onOk(() => {
        canceled.value.delete(offer.address);
    });
};

const handleWithdraw = (offer) => {
    $q.dialog({
        component: defineAsyncComponent(() => import("@/components/Dialogs/OfferWithdrawDialog")),
        componentProps: {
            index: props.index,
            offer,
        },
    }).onOk(() => {
        withdrawn.value.add(offer.address);
    });
};

const handleUpdate = (offer) => {
    $q.dialog({
        component: defineAsyncComponent(() => import("@/components/Dialogs/OfferUpdateDialog")),
        componentProps: {
            index: props.index,
            offer,
        },
    });
};

const handleRevoke = (offer) => {
    $q.dialog({
        component: defineAsyncComponent(() => import("@/components/Dialogs/OfferCancelDialog")),
        componentProps: {
            index: props.index,
            offer,
        },
    }).onOk(() => {
        canceled.value.add(offer.address);
    });
};
</script>
