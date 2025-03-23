<template>
    <q-card :disable="loading" flat>
        <q-linear-progress v-if="loading" query class="absolute-top" />
        <q-checkbox v-model="data.cuda_enable" label="Enable CUDA compute (farming and proof verify)" />
    </q-card>
</template>

<script setup>
const { data, loading } = useConfigData();
</script>
