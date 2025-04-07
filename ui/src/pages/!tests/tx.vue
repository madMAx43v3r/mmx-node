<template>
    <q-page padding>
        <div class="q-gutter-y-sm">
            <q-input v-model="contract" filled type="textarea" input-style="height: 500px" />
            <div class="q-gutter-x-xs">
                <q-btn label="Format" color="secondary" @click="handleFormat" />
                <q-btn label="Validate" color="primary" @click="handleValidate" />
                <q-btn label="Broadcast" color="negative" @click="handleBroadcast" />
            </div>

            <q-inner-loading :showing="transactionValidate.isPending.value || transactionBroadcast.isPending.value">
                <q-spinner-gears color="primary" size="3em" />
            </q-inner-loading>
        </div>
    </q-page>
</template>

<script setup>
import { mdiLogout } from "@mdi/js";

const wallet = ref();

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

import { Transaction } from "@/mmx/wallet/Transaction";
import { JSONbigNative } from "@/mmx/wallet/utils/JSONbigNative";
const contract = ref("{}");

const getPayload = async () => {
    const tx = Transaction.parse(contract.value);
    const payload = JSONbigNative.stringify(tx);
    return payload;
};

import { useTransactionValidate, useTransactionBroadcast } from "@/queries/wapi";

const handleFormat = () => {
    const obj = JSONbigNative.parse(contract.value);
    contract.value = JSONbigNative.stringify(obj, null, 4);
};

const transactionValidate = useTransactionValidate();
const handleValidate = async () => {
    const payload = await getPayload();
    transactionValidate.mutate(payload);
};

const transactionBroadcast = useTransactionBroadcast();
const handleBroadcast = async () => {
    const payload = await getPayload();
    transactionBroadcast.mutate(payload);
};

const handleLogout = () => {
    wallet.value = null;
};
</script>
