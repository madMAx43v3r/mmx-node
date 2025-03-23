<template>
    <q-dialog ref="dialogRef" persistent @show="onDialogShow" @hide="onDialogHide">
        <q-card class="q-dialog-plugin" style="width: 800px; max-width: 100vw">
            <WForm ref="formRef" :data="formData" :default-data="defaultFormData" @submit="handleSubmit">
                <q-toolbar class="bg-primary text-white">
                    <q-toolbar-title class="text-subtitle1">
                        <!-- {{ $t("account_offers.withdraw_from") }} -->
                        <b>Withdraw from {{ offer.address }}</b>
                    </q-toolbar-title>
                </q-toolbar>
                <q-card-section>
                    <div class="q-gutter-y-sm">
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
                                <WBtnSubmit label="Withdraw" />
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
    await handleSend();
    onDialogOK();
};
const onDialogShow = () => {
    if (formRef.value) {
        formRef.value.onDialogShow();
    }
};

const defaultFormData = {
    feeRatio: null,
};

const formRef = ref(null);
const formData = reactive({ ...defaultFormData });

const payload = computed(() => ({
    index: props.index,
    address: props.offer.address,
    options: {
        //memo: "Withdraw from offer",
        fee_ratio: formData.feeRatio,
    },
}));

const { promptPassphrase, isLocked } = useWalletLocker(reactive({ index: toRef(() => props.index) }));
const { isValid, isValidConfirmed, isValidUnlocked, isValidConfirmedUnlocked } = useWalletFormStatusL(
    formRef,
    isLocked
);

import { useWalletOfferWithdraw, useWalletOfferWithdrawFeeEstimate } from "@/queries/wapi";
const {
    data: fee,
    isLoading: feeIsLoading,
    latestError: feeLatestError,
} = useWalletOfferWithdrawFeeEstimate(payload, isValidConfirmedUnlocked);

const walletOfferWithdraw = useWalletOfferWithdraw();
const handleSend = async () => {
    const passphrase = await promptPassphrase();
    const payloadWithPassphrase = { ...payload.value, options: { ...payload.value.options, passphrase } };
    //console.log(payloadWithPassphrase);
    await walletOfferWithdraw.mutateAsync(payloadWithPassphrase);
};
</script>
