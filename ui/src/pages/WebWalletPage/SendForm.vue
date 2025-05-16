<template>
    <q-card flat>
        <WForm ref="formRef" :data="formData" :default-data="defaultFormData" @submit="handleSubmit">
            <q-card-section>
                <div class="q-gutter-y-sm">
                    <q-input
                        v-model="formData.destination"
                        :label="$t('account_send_form.destination_address')"
                        placeholder="mmx1..."
                        :rules="[rules.required, rules.address]"
                        hide-bottom-space
                        input-class="text-bold"
                        :clearable="formData.destination != null"
                    />
                    <div class="row q-gutter-x-sm">
                        <q-input
                            v-model.number="formData.amount"
                            :label="$t('account_send_form.amount')"
                            :rules="[rules.required, rules.amount]"
                            hide-bottom-space
                            input-class="amount-input"
                            class="col-sm-3 col-xs-12"
                        />
                        <AddressTokenSelect
                            v-model="formData.currency"
                            :rules="[rules.required]"
                            :address="address"
                            :label="$t('account_send_form.currency')"
                            :emit-value="false"
                            :clearable="formData.currency != null"
                            class="col"
                        />
                    </div>
                    <q-input
                        v-model="formData.memo"
                        label="Memo"
                        :rules="[rules.memo]"
                        _hide-bottom-space
                        :clearable="formData.memo != null"
                    />

                    <div class="row justify-end q-col-gutter-sm">
                        <TxFeeSelect v-model="formData.feeRatio2" class="col-md-2 col-sm-3 col-xs-6" />
                        <TxFeeInput v-model="fee" class="col-md-2 col-sm-3 col-xs-6" :loading="evaluating" />
                    </div>
                </div>
            </q-card-section>

            <q-card-section>
                <div class="row q-mt-md">
                    <div class="col">
                        <WBtnReset />
                    </div>
                    <div class="col-11">
                        <div class="row justify-end q-gutter-x-sm">
                            <WToggleConfirmed />
                            <WBtnSubmit
                                :label="$t('account_send_form.send')"
                                :icon="mdiBankTransferOut"
                                :loading="evaluating"
                            />
                        </div>
                    </div>
                </div>
            </q-card-section>
        </WForm>
    </q-card>
</template>

<script setup>
import { mdiBankTransferOut } from "@mdi/js";
import rules from "@/helpers/rules";

const props = defineProps({
    wallet: {
        type: Object,
        required: true,
    },
    address: {
        type: String,
        required: false,
        default: null,
    },
});

const defaultFormData = {
    destination: null,
    amount: null,
    currency: null,
    memo: null,
    feeRatio2: null,
};

const formRef = ref(null);
const formData = reactive({ ...defaultFormData });
const { isValid, isValidConfirmed } = useWalletFormStatus(formRef);

const evaluating = shallowRef(false);

import { Wallet } from "@/mmx/wallet/Wallet";
const tx = computedAsync(
    async () => {
        if (!isValidConfirmed.value) return null;

        const options = {
            ...(formData.memo && { memo: formData.memo }),
            fee_ratio: formData.feeRatio2,
            expire_at: -1,
            network: "mainnet",
        };

        const tx = await Wallet.getSendTxAsync(
            props.wallet,
            formData.amount * Math.pow(10, formData.currency.decimals),
            formData.destination,
            formData.currency.value,
            options
        );
        //console.log(tx);
        return tx;
    },
    null,
    {
        onError: (error) => {
            throw error;
        },
        evaluating,
    }
);

const fee = computed(() => tx.value?.aux.feeValue);

const payload = computedAsync(async () => tx.value.toString());

import { useTransactionValidate, useTransactionBroadcast } from "@/queries/wapi";

const transactionValidate = useTransactionValidate();
const transactionBroadcast = useTransactionBroadcast();

// const handleValidate = async () => {
//     transactionValidate.mutate(payload.value);
// };
// const handleBroadcast = async () => {
//     transactionBroadcast.mutate(payload.value);
// };

const handleSubmit = async () => {
    transactionBroadcast.mutate(payload.value);
};
</script>
