<template>
    <div class="q-gutter-y-sm">
        <SwapLiquidUserInfoTable :address="address" />
        <SwapLiquidForm :address="address" />
    </div>
</template>

<script setup>
import SwapLiquidForm from "./SwapLiquidForm.vue";
import SwapLiquidUserInfoTable from "./SwapLiquidUserInfoTable.vue";

const props = defineProps({
    address: {
        type: String,
        required: true,
    },
});
</script>
