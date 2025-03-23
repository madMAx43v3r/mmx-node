<template>
    <q-btn ref="btnRef" type="submit" :color="color" outline :disable="!isValidConfirmed" />
</template>

<script setup>
const formState = inject("formState");
const { confirmed, isValid, isValidConfirmed } = toRefs(formState);

const btnRef = ref(null);
const color = computed(() => {
    return confirmed.value ? (!btnRef.value?.disable ? "positive" : "negative") : "";
});
</script>
