<template>
    <div>
        <q-card flat>
            <WForm ref="formRef" :data="formData" :default-data="defaultFormData" @submit="handleSubmit">
                <q-toolbar :class="{ 'bg-green': buyForm, 'bg-red': !buyForm }" class="text-white" />
                <q-card-section>
                    <div class="row justify-end q-col-gutter-x-md">
                        <q-input
                            :model-value="walletBalance"
                            :label="$t('swap.wallet_ballance')"
                            :suffix="data?.symbols[index]"
                            input-class="amount-input"
                            readonly
                            class="col-6"
                        />
                        <q-input
                            v-model.number="formData.amount"
                            :debounce="200"
                            :label="$t('common.amount')"
                            input-class="amount-input"
                            :suffix="data?.symbols[index]"
                            :rules="[rules.required, rules.amount, hasFunds]"
                            class="col-6"
                        />
                    </div>

                    <div class="row justify-end q-col-gutter-x-md">
                        <q-input
                            :model-value="estTradeFee"
                            :label="$t('swap.trade_fee_estimated')"
                            suffix="%"
                            input-class="amount-input"
                            readonly
                            class="col-6"
                        />
                        <q-input
                            :model-value="estTradeAmount"
                            :label="$t('swap.you_receive_estimated')"
                            :suffix="data?.symbols[indexInv]"
                            input-class="amount-input"
                            readonly
                            class="col-6"
                        />
                    </div>
                    <div class="row justify-end q-col-gutter-x-md">
                        <IterSelect v-model="formData.numIter" class="col" />
                        <q-input
                            :model-value="estTradePrice"
                            label="Trade Price (average, including fee)"
                            :suffix="`${data?.symbols[index]} / ${data?.symbols[indexInv]}`"
                            input-class="amount-input"
                            readonly
                            class="col-8"
                        />
                    </div>

                    <div class="row justify-end q-col-gutter-x-md">
                        <SlippageSelect v-model="formData.slippage" class="col-4" />
                        <TxFeeSelect v-model="formData.feeRatio" class="col-4" />
                        <TxFeeInput
                            v-model="fee"
                            :loading="feeIsLoading"
                            :error="feeLatestError"
                            :locked="isLocked"
                            class="col-4"
                        />
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
                                <WBtnSubmit label="Swap" :icon="mdiBankTransfer" />
                            </div>
                        </div>
                    </div>
                </q-card-section>
            </WForm>
        </q-card>
    </div>
</template>

<script setup>
import { mdiBankTransfer } from "@mdi/js";

import rules from "@/helpers/rules";
import SlippageSelect from "./SlippageSelect.vue";
import IterSelect from "./IterSelect.vue";

const props = defineProps({
    address: {
        type: String,
        required: true,
    },
    buyForm: {
        type: Boolean,
        required: false,
        default: true,
    },
});

const { activeWalletIndex } = useActiveWalletStore();

const index = computed(() => (!props.buyForm ? 0 : 1));
const indexInv = computed(() => (props.buyForm ? 0 : 1));

const defaultFormData = {
    amount: null,
    numIter: 5,
    slippage: 0.98,
    feeRatio: null,
};

const formRef = ref(null);
const formData = reactive({ ...defaultFormData });

const { promptPassphrase, isLocked } = useWalletLocker(reactive({ index: activeWalletIndex }));
const { isValid, isValidConfirmed, isValidUnlocked, isValidConfirmedUnlocked } = useWalletFormStatusL(
    formRef,
    isLocked
);

const estPayload = reactive({
    id: toRef(() => props.address),
    index: toRef(() => index.value),
    amount: toRef(() => formData.amount),
    iters: toRef(() => formData.numIter),
});

import { useSwapTradeEstimate } from "@/queries/wapi";
const { data: estTradeData } = useSwapTradeEstimate(estPayload, isValid);
const estTradeFee = computed(() => estTradeData.value?.fee_percent.toFixed(2));
const estTradePrice = computed(() => {
    const avgPrice = estTradeData.value?.avg_price;
    return avgPrice ? parseFloat((index.value === 1 ? 1 / avgPrice : avgPrice).toPrecision(6)) : null;
});
const estTradeAmount = computed(() => estTradeData.value?.trade.value);

const payload = computed(() => ({
    wallet: activeWalletIndex.value,
    address: props.address,
    index: index.value,
    amount: formData.amount,
    min_trade: formData.slippage * estTradeAmount.value,
    num_iter: formData.numIter,
    options: {
        //memo: "Swap trade",
        fee_ratio: formData.feeRatio,
    },
}));

import { useSwapInfo } from "@/queries/wapi";
const { data } = useSwapInfo(reactive({ id: toRef(() => props.address) }));

import { useWalletBalance } from "@/queries/wapi";
const { data: walletBalance } = useWalletBalance(
    reactive({
        index: activeWalletIndex,
        currency: toRef(() => data.value?.tokens[index.value]),
    }),
    (data) => (data ? data.spendable : 0),
    () => !!data.value?.tokens[index.value]
);

const hasFunds = (value) => value <= walletBalance.value || "Insufficient funds";

import { useWalletSwapTrade, useWalletSwapTradeFeeEstimate } from "@/queries/wapi";
const {
    data: fee,
    isLoading: feeIsLoading,
    latestError: feeLatestError,
} = useWalletSwapTradeFeeEstimate(payload, isValidConfirmedUnlocked);

const walletSwapTrade = useWalletSwapTrade();
const handleSubmit = async () => {
    const passphrase = await promptPassphrase();
    const payloadWithPassphrase = { ...payload.value, options: { ...payload.value.options, passphrase } };
    //console.log(payloadWithPassphrase);
    await walletSwapTrade.mutateAsync(payloadWithPassphrase);
};
</script>
