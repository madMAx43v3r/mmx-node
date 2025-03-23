<template>
    <q-select v-bind="$attrs" :options="slippageItems" emit-value map-options label="Max Slippage" />
</template>

<script setup>
const slippageItems = [
    { label: "0.5 %", value: 0.995 },
    { label: "1 %", value: 0.99 },
    { label: "2 %", value: 0.98 },
    { label: "5 %", value: 0.95 },
    { label: "10 %", value: 0.9 },
];
</script>
