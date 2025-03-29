<template>
    <q-card flat>
        <WForm ref="formRef" :data="formData" :default-data="defaultFormData" @submit="handleSubmit">
            <q-card-section>
                <div class="q-gutter-y-sm">
                    <q-input v-model="formData.name" :rules="[rules.required]" label="New PlotNFT Name" />
                </div>

                <div class="row justify-end q-col-gutter-sm">
                    <TxFeeSelect v-model="formData.feeRatio" class="col-md-2 col-sm-3 col-xs-6" />
                    <TxFeeInput v-model="fee" class="col-md-2 col-sm-3 col-xs-6" />
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
                            <WBtnSubmit :label="$t('common.create')" />
                        </div>
                    </div>
                </div>
            </q-card-section>
        </WForm>
    </q-card>
</template>

<script setup>
import rules from "@/helpers/rules";

const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

const defaultFormData = {
    name: null,
    feeRatio: null,
};

const formRef = ref(null);
const formData = reactive({ ...defaultFormData });

const payload = computed(() => ({
    index: props.index,
    name: formData.name,
    options: {
        fee_ratio: formData.feeRatio,
    },
}));

const { promptPassphrase, isLocked } = useWalletLocker(reactive({ index: toRef(() => props.index) }));
const { isValid, isValidConfirmed, isValidUnlocked, isValidConfirmedUnlocked } = useWalletFormStatusL(
    formRef,
    isLocked
);

import { useWalletPlotnftCreate } from "@/queries/wapi";
const fee = computed(() => (isValidConfirmed.value ? 0.095 : null));

const walletPlotnftCreate = useWalletPlotnftCreate();
const handleSubmit = async () => {
    const passphrase = await promptPassphrase();
    const payloadWithPassphrase = { ...payload.value, options: { ...payload.value.options, passphrase } };
    await walletPlotnftCreate.mutateAsync(payloadWithPassphrase);
};
</script>
