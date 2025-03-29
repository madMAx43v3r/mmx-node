<template>
    <q-page padding>
        <template v-if="!wallet">
            <SeedInputForm v-model:wallet="wallet" />
        </template>
        <template v-else>
            <transition appear enter-active-class="animate__animated animate__fadeIn">
                <div v-if="address" class="q-gutter-y-sm">
                    <div>
                        <div class="row q-col-gutter-xs">
                            <m-chip>{{ $t("common.address") }}</m-chip>
                            <m-chip copy>{{ address }}</m-chip>
                            <m-chip
                                square
                                outline
                                :ripple="false"
                                class="col-auto no-padding"
                                style="border-style: none !important"
                            >
                                <q-btn
                                    label="Logout"
                                    :icon="mdiLogout"
                                    color="secondary"
                                    size="sm"
                                    outline
                                    @click="handleLogout"
                                />
                            </m-chip>
                        </div>
                    </div>
                    <q-card flat>
                        <WForm ref="formRef" :data="formData" :default-data="defaultFormData">
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
                                            :label="$t('account_send_form.amount') + ' in mojos'"
                                            :rules="[rules.required, rules.amount]"
                                            hide-bottom-space
                                            input-class="amount-input"
                                            class="col-sm-3 col-xs-12"
                                        />

                                        <q-input
                                            v-model="formData.currency"
                                            :rules="[rules.required, rules.address]"
                                            :address="formData.currency"
                                            :label="$t('account_send_form.currency')"
                                            :clearable="formData.currency != null"
                                            class="col"
                                        />
                                    </div>
                                </div>
                                <q-input
                                    v-model="formData.memo"
                                    label="Memo"
                                    :rules="[rules.memo]"
                                    hide-bottom-space
                                    :clearable="formData.memo != null"
                                />

                                <div class="row justify-end q-col-gutter-sm">
                                    <TxFeeSelect v-model="formData.feeRatio2" class="col-md-2 col-sm-3 col-xs-6" />
                                    <TxFeeInput
                                        v-model="fee"
                                        class="col-md-2 col-sm-3 col-xs-6"
                                        :loading="evaluating"
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
                                        </div>
                                    </div>
                                </div>
                            </q-card-section>
                        </WForm>
                    </q-card>
                    <q-card flat>
                        <q-card-section v-if="qrData">
                            <q-input
                                :model-value="tx?.toString()"
                                filled
                                type="textarea"
                                input-style="height: 200px"
                                readonly
                            />
                            <img :src="qrCode" alt="QR Code" /><br />
                            <a :href="qrData" target="_blank" rel="noopener noreferrer" class="text-primary">QR Link</a>
                            <div>Size: {{ qrDataSize }} bytes</div>
                        </q-card-section>
                    </q-card>

                    <q-inner-loading :showing="evaluating">
                        <q-spinner-cube size="50px" color="primary" />
                    </q-inner-loading>
                </div>
            </transition>

            <q-inner-loading :showing="!address">
                <q-spinner-cube size="50px" color="primary" />
            </q-inner-loading>
        </template>
    </q-page>
</template>

<script setup>
import { mdiLogout } from "@mdi/js";
import rules from "@/helpers/rules";

import { storeToRefs } from "pinia";
const walletStore = useWalletStore();
const { wallet } = storeToRefs(walletStore);

const address = computedAsync(
    async () => {
        return wallet.value && (await wallet.value.getAddressAsync(0));
    },
    null,
    {
        onError: (error) => {
            throw error;
        },
    }
);

const defaultFormData = {
    destination: "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6",
    amount: 1,
    currency: "mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev",
    memo: "qr code tx",
    feeRatio2: null,
};

const formRef = ref(null);
const formData = reactive({ ...defaultFormData });
const { isValid, isValidConfirmed } = useWalletFormStatus(formRef);

import { Wallet } from "@/mmx/wallet/Wallet";
const evaluating = ref(false);
const tx = computedAsync(
    async () => {
        if (!wallet.value) return null;
        if (!isValidConfirmed.value) return null;

        const options = {
            ...(formData.memo && { memo: formData.memo }),
            fee_ratio: formData.feeRatio2,
            expire_at: -1,
            network: "mainnet",
        };

        const tx = await Wallet.getSendTxAsync(
            wallet.value,
            formData.amount,
            formData.destination,
            formData.currency,
            options
        );
        // console.log(tx);
        // console.log(tx.toString().length);
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

const fee = computed(() => tx.value?.aux?.feeValue);
const payload = computed(() => tx.value?.toString());

import { useTransactionValidate, useTransactionBroadcast } from "@/queries/wapi";

const transactionValidate = useTransactionValidate();
const handleValidate = async () => {
    transactionValidate.mutate(payload.value);
};

const transactionBroadcast = useTransactionBroadcast();
const handleBroadcast = async () => {
    transactionBroadcast.mutate(payload.value);
};

const handleLogout = () => {
    wallet.value = null;
};

import router from "@/plugins/router";
import { useQRCode } from "@vueuse/integrations/useQRCode";
import "@/mmx/wallet/Transaction.ext";
const qrData = computed(() => {
    const txData = tx.value?.toCompressedBase64();
    if (!txData) return;

    // eslint-disable-next-line no-undef
    const baseUrl = __TX_QR_SEND_BASE_URL__ || window.location.origin;

    const route = router.resolve({ name: "tx-qr-send", params: { txData } });
    const url = new URL(route.href, baseUrl);
    const result = url.toString();

    // console.log(result);

    // const routeTmp = router.resolve({ name: "tx-qr-send", params: { txData: "0" } });
    // const urlTmp = new URL(routeTmp.href, baseUrl);
    // console.log(`result size: ${result.length - (urlTmp.toString().length - 1)}; `);
    // console.log(`txData size: ${txData.length}; `);

    return result;
});

const qrDataSize = computed(() => qrData.value?.length);

const qrOptions = {
    errorCorrectionLevel: "L",
};
const qrCode = useQRCode(qrData, qrOptions);
</script>
