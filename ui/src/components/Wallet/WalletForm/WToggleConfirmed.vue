<template>
    <q-toggle v-model="formState.confirmed" :label="$t('common.confirm')" />
</template>

<script setup>
const formState = inject("formState");
</script>
