<template>
    <div class="q-gutter-y-sm">
        <AccountBalanceTable :index="index" />
        <AccountHistoryTable :index="index" :limit="50" />
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
