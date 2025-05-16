<template>
    <x-select v-bind="$attrs" :options="options" />
</template>

<script setup>
const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
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

    balances.value.map((token) => {
        const label = token.symbol;
        const value = token.contract;

        const description = token.contract;
        const descriptionClasses = "mono";

        const isNative = token.is_native;

        if (!isNative) {
            res.push({ label, value, description, descriptionClasses });
        } else {
            res.push({ label, value });
        }
    });

    return res;
});

import { useWalletBalance } from "@/queries/wapi";
const { rows: balances, loading } = useWalletBalance(
    reactive({ index: toRef(() => props.index) }),
    (data) => data.balances
);
</script>
