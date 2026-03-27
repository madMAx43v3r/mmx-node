<template>
    <q-select v-model="feeRate" :options="options" emit-value map-options label="TX Fee Ratio" />
</template>

<script setup>
const feeRate = defineModel({
    type: Number,
    required: false,
    default: 1024,
});

const options = computed(() => {
    return [
        { label: "1x (min fee)", value: 1024 },
        { label: "2x", value: 2048 },
        { label: "3x", value: 3072 },
        { label: "5x", value: 5120 },
        { label: "10x", value: 10240 },
        { label: "20x", value: 20480 },
    ];
});

watchEffect(() => {
    if (!feeRate.value) {
        feeRate.value = 1024;
    }
});
</script>
