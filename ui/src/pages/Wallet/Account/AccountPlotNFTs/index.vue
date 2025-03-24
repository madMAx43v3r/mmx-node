<template>
    <div class="q-gutter-y-sm q-mt-sm">
        <AccountPlotNFTsCreate v-bind="props" />
        <div class="q-gutter-y-sm">
            <q-linear-progress v-if="loading" query />
            <template v-for="item in contracts" :key="item.address">
                <AccountPlotNFTsControl :address="item.address" />
            </template>
        </div>
    </div>
</template>

<script setup>
import AccountPlotNFTsCreate from "./AccountPlotNFTsCreate.vue";
import AccountPlotNFTsControl from "./AccountPlotNFTsControl.vue";

const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

import { useChainInfo, useWalletContracts } from "@/queries/wapi";
const { rows: chainInfo } = useChainInfo();
const plotnftBinary = toRef(() => chainInfo.value?.plot_nft_binary);

const params = reactive({
    index: toRef(() => props.index),
    type_hash: toRef(() => plotnftBinary.value),
    owned: true,
});
const { rows: contracts, loading } = useWalletContracts(params, () => !!plotnftBinary.value);
</script>
