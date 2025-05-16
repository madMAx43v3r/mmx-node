<template>
    <x-select v-model="wallet" v-bind="$attrs" :options="options" />
</template>

<script setup>
const wallet = defineModel({
    type: [Number, Object],
    required: false,
    default: null,
});

const props = defineProps({
    nullItem: {
        type: Boolean,
        required: false,
        default: false,
    },
    nullLabel: {
        type: String,
        required: false,
        default: "null",
    },
});

const { t } = useI18n();
const options = computed(() => {
    const res = [];
    if (props.nullItem) {
        res.push({ label: props.nullLabel, value: null });
    }
    accounts.value.map((account) => {
        const label = `${t("market_menu.wallet")} #${account.account}`;
        const value = account.account;

        const address = account.address;
        const description = account.address;
        const descriptionClasses = "mono";

        res.push({ label, value, address, description, descriptionClasses });
    });
    return res;
});

import { useWalletAccounts } from "@/queries/wapi";
const { rows: accounts } = useWalletAccounts();

watchEffect(() => {
    if (accounts.value.length > 0 && !wallet.value) {
        wallet.value = options.value[0].value;
    }
});
</script>
