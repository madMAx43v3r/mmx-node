<template>
    <div class="q-gutter-y-sm">
        <BalanceTable v-if="source" :address="source" show-empty />
        <AccountBalanceTable v-else :index="index" />
        <AccountSendForm v-bind="props" />
        <AccountTxHistoryTable :index="index" :limit="10" />
    </div>
</template>

<script setup>
import AccountSendForm from "./AccountSendForm.vue";

const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
    source: {
        type: String,
        required: false,
        default: null,
    },
    target: {
        type: String,
        required: false,
        default: null,
    },
});
</script>
