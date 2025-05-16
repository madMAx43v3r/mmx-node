<template>
    <div class="q-gutter-y-sm q-mt-sm">
        <CreateContractMenu :index="index" />
        <AccountContractsIndex :index="index" />
    </div>
</template>
<script setup>
import AccountContractsIndex from "./AccountContractsIndex.vue";
import CreateContractMenu from "./CreateContractMenu.vue";

const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});
</script>
