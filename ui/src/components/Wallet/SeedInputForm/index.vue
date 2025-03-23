<template>
    <q-card flat>
        <q-badge v-if="errorMsg" color="negative" floating>{{ errorMsg }}</q-badge>
        <WForm
            ref="formRef"
            :data="formData"
            :default-data="defaultFormData"
            @reset="handleReset"
            @submit="handleSubmit"
        >
            <q-card-section>
                <div class="q-gutter-y-sm">
                    <SeedInput v-model="formData.mnemonic" :debounce="200" />

                    <div class="q-gutter-y-sm">
                        <q-checkbox v-model="formData.withPassphrase" :label="$t('create_wallet.use_passphrase')" />
                        <template v-if="formData.withPassphrase">
                            <q-input
                                v-model="formData.passphrase"
                                :debounce="200"
                                :label="$t('create_wallet.passphrase')"
                                outlined
                                :type="!showPassphrase ? 'password' : 'text'"
                                autocomplete="new-password"
                            >
                                <template v-slot:append>
                                    <q-icon
                                        :name="!showPassphrase ? mdiEyeOff : mdiEye"
                                        class="cursor-pointer"
                                        @click="showPassphrase = !showPassphrase"
                                    />
                                </template>
                            </q-input>
                        </template>
                    </div>
                </div>
            </q-card-section>

            <q-card-section>
                <div class="row">
                    <div class="col q-my-auto">
                        <div class="row q-gutter-x-sm">
                            <WBtnReset />
                        </div>
                    </div>

                    <div class="col-md-6 col-sm-6 col-xs-10">
                        <div class="row justify-end q-gutter-x-sm">
                            <q-btn
                                label="New mnemonic"
                                outline
                                class="col q-my-auto text-warning"
                                @click="handleNewMnemonic"
                            />
                            <div class="col-6">
                                <div class="row rounded-borders q-px-xs" style="border: 1px dotted">
                                    <q-input
                                        v-model="fingerprint"
                                        readonly
                                        label="Fingerprint"
                                        stack-label
                                        borderless
                                        input-class="text-bold"
                                        class="col"
                                    />

                                    <WBtnSubmit
                                        label="Login"
                                        :disable="!isValid"
                                        class="text-positive q-my-auto float-right"
                                    />
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </q-card-section>
        </WForm>
    </q-card>
</template>

<script setup>
import { mdiEye, mdiEyeOff } from "@mdi/js";

const emit = defineEmits(["login"]);

const modelMnemonic = defineModel("mnemonic", {
    type: String,
    require: false,
    default: null,
});

const modelPassphrase = defineModel("passphrase", {
    type: String,
    require: false,
    default: null,
});

const modelWallet = defineModel("wallet", {
    type: Object,
    require: false,
    default: null,
});

const showPassphrase = ref(false);

const defaultFormData = {
    mnemonic: "",
    passphrase: "",
    withPassphrase: false,
};

const formRef = ref(null);
const formData = reactive({
    ...defaultFormData,
    mnemonic: modelMnemonic.value?.trim(),
    passphrase: modelPassphrase.value?.trim(),
    withPassphrase: !!modelPassphrase.value,
});

watchEffect(() => {
    if (!formData.withPassphrase) {
        formData.passphrase = "";
    }
});

const { isValid: isValidForm } = useWalletFormStatusL(formRef);
const isValid = computed(() => isValidForm.value && wallet.value != null);

import { ECDSA_Wallet } from "@/mmx/wallet/ECDSA_Wallet";
const tmpWallet = computed(() => {
    const result = { wallet: null, error: "" };
    if (!isEmpty(formData.mnemonic) && isValidForm.value) {
        try {
            result.wallet = new ECDSA_Wallet(formData.mnemonic, formData.passphrase);
        } catch (e) {
            result.error = e.message;
        }
    }
    return result;
});
const wallet = toRef(() => tmpWallet.value.wallet);
const error = toRef(() => tmpWallet.value.error);
const errorMsg = computed(() => error.value.split(".")[0]); // trim long error messages

watch(
    tmpWallet,
    () => {
        if (isValid.value) {
            formRef.value.validate();
        }
    },
    { deep: true }
);

const fingerprint = computedAsync(
    async () => {
        return wallet.value && (await wallet.value.getFingerPrintAsync());
    },
    null,
    {
        onError: (error) => {
            throw error;
        },
    }
);

const handleReset = () => {
    formData.mnemonic = "";
    formData.passphrase = "";

    modelMnemonic.value = null;
    modelPassphrase.value = null;
    modelWallet.value = null;
};

const handleSubmit = async () => {
    modelMnemonic.value = formData.mnemonic;
    modelPassphrase.value = formData.passphrase;
    modelWallet.value = wallet.value;
    emit("login");
};

import { randomWords } from "@/mmx/wallet/mnemonic";
const $q = useQuasar();
const handleNewMnemonic = async () => {
    const mnemonic = randomWords();
    $q.dialog({
        component: defineAsyncComponent(() => import("@/components/Dialogs/ShowSeedDialog")),
        componentProps: {
            seed: mnemonic,
        },
    }).onOk(() => {
        formData.mnemonic = mnemonic;
    });
};

defineExpose({
    onDialogShow: () => formRef.value.onDialogShow,
});
</script>
