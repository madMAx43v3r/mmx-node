<template>
    <q-input :model-value="price" :label="$t('common.price')" :suffix="symbols" input-class="amount-input" readonly />
</template>

<script setup>
const props = defineProps({
    amount1: {
        type: Number,
        required: false,
        default: null,
    },
    symbol1: {
        type: String,
        required: false,
        default: null,
    },
    amount2: {
        type: Number,
        required: false,
        default: null,
    },
    symbol2: {
        type: String,
        required: false,
        default: null,
    },
    invert: {
        type: Boolean,
        required: false,
        default: false,
    },
    format: {
        type: Function,
        required: false,
        default: (value) => parseFloat(value.toPrecision(6)),
    },
});

const price = computed(() => {
    if (props.amount1 && props.amount2) {
        return props.format(!props.invert ? props.amount1 / props.amount2 : props.amount2 / props.amount1);
    }
    return "";
});

const symbols = computed(() => {
    if (props.symbol1 && props.symbol2) {
        const tmp = [props.symbol1, props.symbol2];
        return (!props.invert ? tmp : tmp.slice().reverse()).join(" / ");
    }
    return "";
});
</script>
