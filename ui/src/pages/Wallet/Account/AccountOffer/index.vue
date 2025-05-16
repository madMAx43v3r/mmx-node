<template>
    <div class="q-gutter-y-sm">
        <AccountBalanceTable :index="props.index" />
        <AccountOfferForm :index="props.index" />
        <AccountOfferTable :index="props.index" />
    </div>
</template>

<script setup>
import AccountOfferForm from "./AccountOfferForm.vue";
import AccountOfferTable from "./AccountOfferTable.vue";

const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});
</script>
