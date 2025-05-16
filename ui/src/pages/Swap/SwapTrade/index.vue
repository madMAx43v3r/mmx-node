<template>
    <div class="q-mt-sm">
        <div class="row q-col-gutter-sm">
            <SwapTradeForm :address="address" :buy-form="true" class="col-sm-12 col-md-6" />
            <SwapTradeForm :address="address" :buy-form="false" class="col-sm-12 col-md-6" />
        </div>
    </div>
</template>

<script setup>
import SwapTradeForm from "./SwapTradeForm/index.vue";

const props = defineProps({
    address: {
        type: String,
        required: true,
    },
});
</script>
