<template>
    <w-chart :loading="loading" :option="option" />
</template>

<script setup>
const props = defineProps({
    limit: {
        type: Number,
        default: 1000,
    },
    step: {
        type: Number,
        default: 90,
    },
});

const { t } = useI18n();

const select = (data) => {
    var result = [];
    for (const item of data) {
        result.push([item.height, item.netspace]);
    }
    return result;
};

import { useGraphBlocks } from "@/queries/wapi";
const { data, loading } = useGraphBlocks(props, select);

const option = useEChartsOption();
option.title.text = t("netspace_graph.title", ["PB"]);
option.series[0].data = data;
</script>
