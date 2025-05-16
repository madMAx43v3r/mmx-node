<template>
    <x-select v-bind="$attrs" :options="options" />
</template>

<script setup>
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

    tokens.value.map((token) => {
        const label = token.symbol;
        const value = token.currency;
        const decimals = token.decimals;

        const description = token.currency;
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

import { useWalletTokens } from "@/queries/wapi";
const { rows: tokens, loading } = useWalletTokens();
</script>
