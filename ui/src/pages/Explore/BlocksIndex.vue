<template>
    <q-card flat>
        <BlockTable :rows="rows" :loading="loading" />
    </q-card>
</template>

<script setup>
const props = defineProps({
    limit: {
        type: Number,
        default: 20,
    },
});

import { useHeaders } from "@/queries/wapi";
const { rows, loading } = useHeaders(props);
</script>
