<template>
    <q-form ref="formRef" @submit="_handleSubmit" @reset="_handleReset">
        <slot />
    </q-form>
</template>

<script setup>
const props = defineProps({
    data: {
        type: Object,
        required: true,
    },
    defaultData: {
        type: Object,
        required: true,
    },
    onSubmit: {
        type: Function,
        required: false,
        default: () => {},
    },
    onReset: {
        type: Function,
        required: false,
        default: () => {},
    },
});

import { useWalletForm } from "./useWalletForm";
const {
    formRef,
    confirmed,
    isValid,
    isValidConfirmed,
    reset: _reset,
    onDialogShow,
    validate,
} = useWalletForm(props.data, props.defaultData);

const reset = () => {
    _reset();
    props.onReset();
};

const _handleSubmit = async () => {
    confirmed.value = false;
    props.onSubmit();
};
const _handleReset = async () => {
    reset();
};

const formState = reactive({
    confirmed,
    isValid,
    isValidConfirmed,
});

provide("formState", formState);

defineExpose({
    reset,
    confirmed,
    isValid,
    isValidConfirmed,
    onDialogShow,
    validate,
});
</script>
