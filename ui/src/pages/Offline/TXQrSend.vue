<template>
    <q-page padding>
        <div class="q-gutter-y-sm">
            <highlightjs :code="tx?.toString(null, 4)" class="hljsCode" />
            <div class="q-gutter-x-xs">
                <q-btn :disable="!tx" label="Validate" color="primary" @click="handleValidate" />
                <q-btn :disable="!tx" label="Broadcast" color="primary" @click="handleBroadcast" />
            </div>
        </div>
    </q-page>
</template>

<script setup>
const props = defineProps({
    txData: {
        type: String,
        required: true,
    },
});

import { Transaction } from "@/mmx/wallet/Transaction";
import "@/mmx/wallet/Transaction.ext";

const tx = computed(() => Transaction.fromCompressedBase64(props.txData));

const payload = computed(() => tx.value?.toString());

import { useTransactionValidate, useTransactionBroadcast } from "@/queries/wapi";
const transactionValidate = useTransactionValidate();

const handleValidate = () => {
    transactionValidate.mutate(payload.value);
};

const transactionBroadcast = useTransactionBroadcast();
const handleBroadcast = () => {
    transactionBroadcast.mutate(payload.value);
};
</script>

<style scoped>
::v-deep(pre.hljsCode) {
    font-size: 0.8em;
}
</style>
