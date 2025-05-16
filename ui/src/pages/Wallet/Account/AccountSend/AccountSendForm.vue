<template>
    <q-card flat>
        <WForm
            ref="formRef"
            :data="formData"
            :default-data="defaultFormData"
            @reset="handleReset"
            @submit="handleSubmit"
        >
            <q-card-section>
                <div class="q-gutter-y-sm">
                    <q-input
                        v-if="!!source"
                        :model-value="source"
                        :label="$t('account_send_form.source_address')"
                        readonly
                    />

                    <WalletSelect
                        v-if="!target"
                        v-model="wallet"
                        :label="$t('account_send_form.destination')"
                        null-item
                        :null-label="$t('account_send_form.address_input')"
                        :emit-value="false"
                        :clearable="wallet?.value != null"
                        :disable="!!target"
                    />

                    <q-input
                        v-model="formData.destination"
                        :label="$t('account_send_form.destination_address')"
                        placeholder="mmx1..."
                        :rules="[rules.required, rules.address]"
                        hide-bottom-space
                        input-class="text-bold"
                        :readonly="!!wallet?.value || !!target"
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
                            v-if="!!source"
                            v-model="formData.currency"
                            :rules="[rules.required]"
                            :address="source"
                            :label="$t('account_send_form.currency')"
                            :clearable="formData.currency != null"
                            class="col"
                        />

                        <BalanceTokenSelect
                            v-else
                            v-model="formData.currency"
                            :rules="[rules.required]"
                            :index="index"
                            :label="$t('account_send_form.currency')"
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
                            <WBtnSubmit :label="$t('account_send_form.send')" :icon="mdiBankTransferOut" />
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
    index: {
        type: Number,
        required: true,
    },
    source: {
        type: String,
        required: false,
        default: null,
    },
    target: {
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
    feeRatio: null,
};

const formRef = ref(null);
const formData = reactive({ ...defaultFormData });

const wallet = ref(null);

const initDestination = () => {
    if (props.target) {
        formData.destination = props.target;
    } else {
        formData.destination = wallet.value && wallet.value?.address;
    }
};

watchEffect(() => {
    initDestination();
});

const handleReset = async () => {
    wallet.value = null;
    initDestination();
};

const payload = computed(() => ({
    index: props.index,
    amount: formData.amount,
    currency: formData.currency,
    ...(props.source && { src_addr: props.source }),
    dst_addr: formData.destination,
    options: {
        ...(formData.memo && { memo: formData.memo }),
        fee_ratio: formData.feeRatio,
    },
}));

const { promptPassphrase, isLocked } = useWalletLocker(reactive({ index: toRef(() => props.index) }));
const { isValid, isValidConfirmed, isValidUnlocked, isValidConfirmedUnlocked } = useWalletFormStatusL(
    formRef,
    isLocked
);

import { useWalletSend, useWalletSendFeeEstimate } from "@/queries/wapi";
const {
    data: fee,
    isLoading: feeIsLoading,
    latestError: feeLatestError,
} = useWalletSendFeeEstimate(payload, isValidConfirmedUnlocked);

const walletSend = useWalletSend();
const handleSubmit = async () => {
    const passphrase = await promptPassphrase();
    const payloadWithPassphrase = { ...payload.value, options: { ...payload.value.options, passphrase } };
    await walletSend.mutateAsync(payloadWithPassphrase);
};
</script>
