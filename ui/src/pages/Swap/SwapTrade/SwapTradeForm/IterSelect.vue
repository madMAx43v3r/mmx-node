<template>
    <q-select v-bind="$attrs" :options="iterItems" emit-value map-options label="Iterations" />
</template>

<script setup>
const iterItems = [1, 5, 20, 100];
</script>
