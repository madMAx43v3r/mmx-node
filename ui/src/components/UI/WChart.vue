<template>
    <v-chart v-if="!loading" :option="option" autoresize class="chart" />
    <q-skeleton v-else square class="chart" />
</template>

<script setup>
import VChart from "vue-echarts";
const props = defineProps({
    loading: {
        type: Boolean,
        default: false,
    },
    option: {
        type: Object,
        default: null,
    },
});
</script>

<style scoped>
.chart {
    width: 100%;
    height: 450px;
}
</style>
