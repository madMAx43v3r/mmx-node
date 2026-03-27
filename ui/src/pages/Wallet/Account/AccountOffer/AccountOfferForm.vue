<template>
    <q-card flat>
        <WForm ref="formRef" :data="formData" :default-data="defaultFormData" @submit="handleSubmit">
            <q-card-section>
                <div class="q-gutter-y-sm">
                    <div class="row q-gutter-x-sm">
                        <q-input
                            v-model.number="formData.bidAmount"
                            :label="$t('account_offer_form.offer_amount')"
                            input-class="amount-input"
                            :rules="[rules.required, rules.amount]"
                            class="col-sm-3 col-xs-12"
                        />
                        <BalanceTokenSelect
                            v-model="formData.bidCurrency"
                            :emit-value="false"
                            :rules="[rules.required]"
                            :index="index"
                            :label="$t('account_offer_form.offer_currency')"
                            :clearable="formData.bidCurrency != null"
                            class="col"
                        />
                    </div>
                    <div class="row q-gutter-x-sm">
                        <q-input
                            v-model.number="formData.price"
                            :label="$t('account_offers.price')"
                            :suffix="priceSuffix"
                            input-class="amount-input"
                            :rules="[rules.required, rules.amount]"
                            class="price col-sm-3 col-xs-12"
                        />
                        <TokenSelect
                            v-model="formData.askCurrency"
                            :emit-value="false"
                            :rules="[rules.required]"
                            :index="index"
                            :label="$t('account_offer_form.receive_currency')"
                            :clearable="formData.askCurrency != null"
                            class="col"
                        />
                    </div>

                    <div class="row justify-end q-col-gutter-sm">
                        <TxFeeSelect v-model="formData.feeRatio" class="col-md-2 col-sm-3 col-xs-6" />
                        <TxFeeInput
                            v-model="fee"
                            :loading="feeIsLoading"
                            :error="feeLatestError"
                            :locked="isLocked"
                            class="col-md-2 col-sm-3 col-xs-6"
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
                            <WBtnSubmit :label="$t('account_offer_form.offer')" :icon="mdiHandCoin" />
                        </div>
                    </div>
                </div>
            </q-card-section>
        </WForm>
    </q-card>
</template>
<script setup>
import { mdiHandCoin } from "@mdi/js";
import rules from "@/helpers/rules";
const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

const defaultFormData = {
    bidAmount: null,
    bidCurrency: null,
    price: null,
    askCurrency: null,
    feeRatio: null,
};

const formRef = ref(null);
const formData = reactive({ ...defaultFormData });

const priceSuffix = computed(() => {
    const { bidCurrency, askCurrency } = formData;
    return bidCurrency && askCurrency ? `${bidCurrency.label} / ${askCurrency.label}` : null;
});

const payload = computed(() => ({
    index: props.index,
    bid: formData.bidAmount,
    bid_currency: formData.bidCurrency?.value,
    price: formData.price,
    ask_currency: formData.askCurrency?.value,
    options: {
        fee_ratio: formData.feeRatio,
    },
}));

const { promptPassphrase, isLocked } = useWalletLocker(reactive({ index: toRef(() => props.index) }));
const { isValid, isValidConfirmed, isValidUnlocked, isValidConfirmedUnlocked } = useWalletFormStatusL(
    formRef,
    isLocked
);

import { useWalletMakeOffer, useWalletMakeOfferFeeEstimate } from "@/queries/wapi";
const {
    data: fee,
    isLoading: feeIsLoading,
    latestError: feeLatestError,
} = useWalletMakeOfferFeeEstimate(payload, isValidConfirmedUnlocked);

const walletMakeOffer = useWalletMakeOffer();
const handleSubmit = async () => {
    const passphrase = await promptPassphrase();
    const payloadWithPassphrase = { ...payload.value, options: { ...payload.value.options, passphrase } };
    await walletMakeOffer.mutateAsync(payloadWithPassphrase);
};
</script>

<style scoped>
::v-deep(.price .q-field__suffix) {
    font-weight: bold;
}
</style>
