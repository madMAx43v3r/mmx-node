<template>
    <div class="q-gutter-y-sm q-mt-sm">
        <q-card flat>
            <q-card-section class="q-gutter-x-xs">
                <q-form @submit="handleUpdate">
                    <div class="row q-gutter-x-sm">
                        <q-input
                            v-model="formData.name"
                            :label="t('create_wallet.account_name')"
                            readonly
                            class="col"
                        />
                        <q-input
                            v-model.number="formData.num_addresses"
                            :label="t('create_wallet.number_of_addresses')"
                            class="col-3"
                        />
                    </div>
                    <div class="q-mt-md row justify-end">
                        <q-btn label="Update" type="submit" color="positive" outline class="col-2" />
                    </div>
                </q-form>
            </q-card-section>
        </q-card>

        <q-card flat>
            <q-card-section class="q-gutter-x-xs">
                <q-btn :label="$t('account_actions.reset_cache')" color="secondary" outline @click="handleResetCache" />

                <q-btn :label="$t('account_actions.show_seed')" color="secondary" outline @click="handleShowSeed" />
                <q-btn v-if="index >= 100" color="negative" outline @click="handleRemove">Remove</q-btn>
            </q-card-section>
        </q-card>
    </div>
</template>

<script setup>
import { mdiClipboard } from "@mdi/js";
const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

const { t } = useI18n();
const $q = useQuasar();
const router = useRouter();

import { useWalletAccount, useWalletSeed } from "@/queries/wapi";
const { rows: account, loading } = useWalletAccount(props);

const formData = reactive({
    name: "",
    num_addresses: 1,
});
watchEffect(() => {
    formData.name = account.value.name;
    formData.num_addresses = account.value.num_addresses;
});

import { useRemoveAccount, useResetCache, useSetAddressCount } from "@/queries/api";
const removeAccount = useRemoveAccount();
const resetCache = useResetCache();

const setAddressCount = useSetAddressCount();
const handleUpdate = () => {
    setAddressCount.mutate({ index: props.index, count: formData.num_addresses });
};

const handleResetCache = () => {
    resetCache.mutate(props.index);
};

const walletSeed = useWalletSeed();
const handleShowSeed = async () => {
    await walletSeed.mutateAsync(props, {
        onSuccess: (result) => {
            $q.dialog({
                component: defineAsyncComponent(() => import("@/components/Dialogs/ShowSeedDialog")),
                componentProps: {
                    seed: result["string"],
                    withPassphrase: account.value.with_passphrase,
                    fingerPrint: account.value.finger_print,
                },
            });
        },
    });
};

const handleRemove = () => {
    $q.dialog({
        title: "Remove Wallet", //TODO i18n
        message: "To recover any funds you will need to re-create the wallet from a stored backup!", //TODO i18n
        cancel: true,
        persistent: true,
        ok: {
            label: "Remove", //TODO i18n
            color: "negative",
        },
    }).onOk(() => {
        const keyFile = account.value.key_file;
        const params = { index: account.value.account, account: account.value.index };
        removeAccount.mutate(params, {
            onSuccess: () => showRemoveWarning(keyFile),
        });
    });
};

const showRemoveWarning = (keyFile) => {
    $q.dialog({
        type: "warning",
        title: "Wallet removed", //TODO i18n
        message: `Wallet file ${keyFile} needs to be deleted manually`, //TODO i18n
        persistent: true,
        cancel: false,
    }).onOk(() => {
        router.push("/wallet/");
    });
};
</script>
