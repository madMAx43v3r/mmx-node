<template>
    <div class="q-gutter-y-sm">
        <q-card flat>
            <q-card-section>
                <div class="row q-gutter-x-sm">
                    <div class="col-8">
                        <q-input v-model="name" :label="$t('create_wallet.account_name')" />
                    </div>
                    <div class="col">
                        <q-input v-model.number="numAddresses" :label="$t('create_wallet.number_of_addresses')" />
                    </div>
                </div>
            </q-card-section>
        </q-card>

        <q-card flat>
            <q-card-section>
                <div class="q-gutter-y-sm">
                    <q-checkbox v-model="withSeed" :label="$t('create_wallet.use_custom_seed')" />
                    <template v-if="withSeed">
                        <div class="text-subtitle1">{{ $t("create_wallet.seed_words") }}</div>
                        <SeedInput v-model="seed" />
                    </template>
                </div>
            </q-card-section>
        </q-card>

        <q-card flat>
            <q-card-section>
                <div class="q-gutter-y-sm">
                    <q-checkbox v-model="withPassphrase" :label="$t('create_wallet.use_passphrase')" />
                    <template v-if="withPassphrase">
                        <q-input
                            v-model="passphrase"
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
                        <q-input
                            v-if="withSeed"
                            v-model="fingerprint"
                            label="Fingerprint (optional, to validate passphrase)"
                            outlined
                        />
                    </template>
                </div>
            </q-card-section>
        </q-card>

        <div class="row full-width justify-end">
            <q-btn
                :icon="mdiWalletPlus"
                :label="$t('create_wallet.create_wallet')"
                :loading="loading"
                color="primary"
                outline
                @click="handleCreateWallet"
            />
        </div>
    </div>
</template>

<script setup>
import { mdiWalletPlus, mdiEye, mdiEyeOff } from "@mdi/js";

const loading = ref(false);
const withSeed = ref(false);
const withPassphrase = ref(false);
const showPassphrase = ref(false);

const name = ref("");
const numAddresses = ref(1);
const seed = ref();
const passphrase = ref();
const fingerprint = ref();

const router = useRouter();
import { useCreateWallet } from "@/queries/api";
const createWallet = useCreateWallet();

import { usePassphraseValidate } from "@/queries/wapi";
const passphraseValidate = usePassphraseValidate();

const handleCreateWallet = () => {
    if (withSeed.value && withPassphrase.value && fingerprint.value) {
        doPassphraseValidate();
    } else {
        doCreateWallet();
    }
};

const doCreateWallet = () => {
    loading.value = true;
    const payload = {
        config: {
            name: name.value,
            numAddresses: numAddresses.value,
        },
        ...(withSeed.value && { words: seed.value }),
        ...(withPassphrase.value && { passphrase: passphrase.value }),
    };
    createWallet.mutate(payload, {
        onSuccess: () => {
            router.push("/wallet/");
        },
        onSettled: () => {
            loading.value = false;
        },
    });
};

const doPassphraseValidate = () => {
    loading.value = true;
    const payload = {
        words: seed.value,
        finger_print: fingerprint.value,
        passphrase: passphrase.value,
    };
    passphraseValidate.mutate(payload, {
        onSuccess: () => {
            doCreateWallet();
        },
        onSettled: () => {
            loading.value = false;
        },
    });
};
</script>
