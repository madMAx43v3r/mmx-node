<template>
    <q-card flat>
        <q-card-section>
            <q-linear-progress v-if="loading" query class="absolute-top" />
            <q-checkbox v-model="data.cuda_enable" label="Enable CUDA compute (farming and proof verify)" />
        </q-card-section>
    </q-card>
</template>

<script setup>
const { data, loading } = useConfigData();
</script>
