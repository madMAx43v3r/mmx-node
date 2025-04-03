<template>
    <q-page padding>
        <div class="q-gutter-y-sm">
            <template v-if="!wallet">
                <SeedInputForm v-model:wallet="wallet" />
            </template>
            <template v-else>
                <div>
                    <div class="row q-gutter-x-none">
                        <m-chip>{{ $t("common.address") }}</m-chip>
                        <m-chip>{{ address }}</m-chip>
                        <div class="col q-my-auto">
                            <q-btn
                                label="Logout"
                                :icon="mdiLogout"
                                size="sm"
                                class="float-right"
                                @click="handleLogout"
                            />
                        </div>
                    </div>

                    <q-card flat>
                        <BalanceTable :address="address" show-empty />
                    </q-card>
                </div>
                <q-input v-model="contract" filled type="textarea" input-style="height: 500px" />
                <div class="q-gutter-x-xs">
                    <q-btn label="Validate" color="primary" @click="handleValidate" />
                    <q-btn label="Broadcast" color="primary" @click="handleBroadcast" />
                </div>
                <AddressHistoryTable :address="address" :limit="20" show-empty />
            </template>
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

const init = {
    note: "TRANSFER",
    sender: "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6",
    outputs: [
        {
            address: "mmx16aq5vpcmxcrh9xck0z06eqnmr87w5r2j062snjj6g7cvj0thry7q0mp3w6",
            contract: "mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev",
            amount: 1,
            memo: "hello from js",
        },
    ],
};

import { Transaction } from "@/mmx/wallet/Transaction";
import { JSONbigNative } from "@/mmx/wallet/utils/JSONbigNative";
const contract = ref(JSONbigNative.stringify(init, null, 4));

const getPayload = async () => {
    const tx = Transaction.parse(contract.value);
    tx.inputs = [];
    tx.solutions = [];

    const options = {
        //fee_ratio: 2048,
        expire_at: -1,
        network: "mainnet",
    };

    await wallet.value.completeAsync(tx, options);

    const payload = JSONbigNative.stringify(tx, null, 4);
    contract.value = payload;

    return payload;
};

import { useTransactionValidate, useTransactionBroadcast } from "@/queries/wapi";

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
