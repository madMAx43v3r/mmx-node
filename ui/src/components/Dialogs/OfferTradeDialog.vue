<template>
    <q-dialog ref="dialogRef" persistent @show="onDialogShow" @hide="onDialogHide">
        <q-card class="q-dialog-plugin" style="width: 800px; max-width: 100vw">
            <WForm ref="formRef" :data="formData" :default-data="defaultFormData" @submit="handleSubmit">
                <q-toolbar class="bg-primary text-white">
                    <q-toolbar-title class="text-subtitle1">
                        <b>Trade — {{ offer.address }}</b>
                    </q-toolbar-title>
                </q-toolbar>
                <q-card-section>
                    <div class="q-gutter-y-sm">
                        <div class="row justify-end q-col-gutter-sm">
                            <q-input
                                :model-value="walletBalance"
                                :label="$t('market_offers.wallet_ballance')"
                                input-class="amount-input"
                                :suffix="offer.ask_symbol"
                                readonly
                                class="col-md-4 col-sm-5 col-xs-6"
                            />
                            <q-input
                                v-model.number="formData.amount"
                                :debounce="200"
                                :label="$t('market_offers.you_send')"
                                input-class="amount-input"
                                :suffix="offer.ask_symbol"
                                :rules="[rules.required, rules.amount, hasFunds]"
                                class="col-md-4 col-sm-5 col-xs-6"
                            >
                                <template v-slot:append>
                                    <q-btn
                                        :icon="mdiTransferUp"
                                        color="secondary"
                                        round
                                        outline
                                        dense
                                        @click="handleIncrease"
                                    >
                                        <q-tooltip>{{ $t("market_offers.increase") }}</q-tooltip>
                                    </q-btn>
                                </template>
                            </q-input>
                        </div>

                        <div class="row justify-end q-col-gutter-sm">
                            <q-input
                                :model-value="offer.bid_balance_value"
                                label="They Offer"
                                input-class="amount-input"
                                :suffix="offer.bid_symbol"
                                readonly
                                class="col-md-4 col-sm-5 col-xs-6"
                            />
                            <q-input
                                :model-value="tradeEstimateValue"
                                :label="$t('market_offers.you_receive')"
                                input-class="amount-input"
                                :suffix="offer.bid_symbol"
                                readonly
                                class="col-md-4 col-sm-5 col-xs-6"
                            />
                        </div>
                        <div class="row justify-end q-col-gutter-sm">
                            <TxFeeSelect v-model="formData.feeRatio" class="col-md-3 col-sm-4 col-xs-6" />
                            <TxFeeInput
                                v-model="fee"
                                :loading="feeIsLoading"
                                :error="feeLatestError"
                                :locked="isLocked"
                                class="col-md-4 col-sm-5 col-xs-6"
                            />
                        </div>
                    </div>
                </q-card-section>

                <q-card-section>
                    <div class="row">
                        <div class="col">
                            <WBtnReset />
                        </div>
                        <div class="col-11">
                            <div class="row justify-end q-gutter-x-sm">
                                <WToggleConfirmed />
                                <WBtnSubmit :label="$t('market_offers.trade')" :disable="!isValidTrade" />
                                <q-btn :label="$t('common.cancel')" flat @click="onDialogCancel" />
                            </div>
                        </div>
                    </div>
                </q-card-section>
            </WForm>
        </q-card>
    </q-dialog>
</template>

<script setup>
import { mdiTransferUp } from "@mdi/js";
import rules from "@/helpers/rules";

const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
    offer: {
        type: Object,
        required: true,
    },
});

import { useDialogPluginComponent } from "quasar";
defineEmits([...useDialogPluginComponent.emits]);
const { dialogRef, onDialogHide, onDialogOK, onDialogCancel } = useDialogPluginComponent();

const handleSubmit = async () => {
    await handleTrade();
    onDialogOK();
};
const onDialogShow = () => {
    if (formRef.value) {
        formRef.value.onDialogShow();
    }
};

const defaultFormData = {
    amount: null,
    feeRatio: null,
};

const formRef = ref(null);
const formData = reactive({ ...defaultFormData });

const { promptPassphrase, isLocked } = useWalletLocker(reactive({ index: toRef(() => props.index) }));
const { isValid, isValidConfirmed, isValidUnlocked, isValidConfirmedUnlocked } = useWalletFormStatusL(
    formRef,
    isLocked
);

import { useOffersTradeEstimate } from "@/queries/wapi";
const { data: tradeEstimateData } = useOffersTradeEstimate(
    reactive({
        id: toRef(() => props.offer.address),
        amount: computed(() => formData.amount || 0),
    })
);
const tradeEstimateValue = computed(() => tradeEstimateData.value?.trade.value ?? null);
const nextInput = computed(() => tradeEstimateData.value?.next_input.value ?? null);
const handleIncrease = () => {
    formData.amount = parseFloat(nextInput.value);
};

const payload = computed(() => ({
    index: props.index,
    address: props.offer.address,
    amount: formData.amount,
    price: tradeEstimateData.value?.inv_price ?? props.offer.inv_price,
    options: {
        //memo: "Trade offer",
        fee_ratio: formData.feeRatio,
    },
}));

import { useWalletOfferTrade, useWalletOfferTradeFeeEstimate } from "@/queries/wapi";
const {
    data: fee,
    isLoading: feeIsLoading,
    latestError: feeLatestError,
} = useWalletOfferTradeFeeEstimate(payload, isValidConfirmedUnlocked);

const walletOfferTrade = useWalletOfferTrade();
const handleTrade = async () => {
    const passphrase = await promptPassphrase();
    const payloadWithPassphrase = { ...payload.value, options: { ...payload.value.options, passphrase } };
    //console.log(payloadWithPassphrase);
    await walletOfferTrade.mutateAsync(payloadWithPassphrase);
};

import { useWalletBalance } from "@/queries/wapi";
const { data: walletBalance } = useWalletBalance(
    reactive({
        index: toRef(() => props.index),
        currency: toRef(() => props.offer.ask_currency),
    }),
    (data) => (data ? data.spendable : 0)
);

const hasFunds = (value) => value <= walletBalance.value || "Insufficient funds";

const isValidTrade = computed(() => {
    const bidAmount = tradeEstimateData.value ? parseFloat(tradeEstimateData.value.trade.amount) : 0;
    return (
        isValidConfirmed.value &&
        formData.amount > 0 &&
        formData.amount <= walletBalance.value &&
        bidAmount > 0 &&
        bidAmount <= props.offer.bid_balance
    );
});
</script>
