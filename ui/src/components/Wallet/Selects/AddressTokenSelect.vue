<template>
    <x-select v-bind="$attrs" :options="options" />
</template>

<script setup>
const props = defineProps({
    address: {
        type: String,
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
        const decimals = token.decimals || 0;

        const description = token.contract;
        const descriptionClasses = "mono";

        const isNative = token.is_native;

        var item = { label, value, decimals };

        if (!isNative) {
            item = { ...item, description, descriptionClasses };
        }

        res.push(item);
    });

    return res;
});

import { useAddress } from "@/queries/wapi";
const { rows: balances, loading } = useAddress(reactive({ id: toRef(() => props.address) }), (data) => data.balances);
</script>
