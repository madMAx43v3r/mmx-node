<template>
    <div class="q-gutter-y-sm">
        <AccountTxHistoryTable :index="index" :limit="200" />
    </div>
</template>

<script setup>
const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});
</script>
