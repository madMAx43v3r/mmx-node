<template>
    <div>
        <div class="row q-col-gutter-xs">
            <template v-for="tile in infoTiles" :key="tile.field">
                <div class="col-md-3 col-sm-6 col-xs-12 col-12">
                    <InfoTile :tile="tile" :rows="infoRows" :loading="infoLoading" />
                </div>
            </template>

            <template v-for="tile in summaryTiles" :key="tile.field">
                <div class="col-md-3 col-sm-6 col-xs-12 col-12">
                    <InfoTile :tile="tile" :rows="summaryRows" :loading="infoLoading" />
                </div>
            </template>
        </div>
    </div>
</template>

<script setup>
import { useFarmInfo, useFarmBlocksSummary } from "@/queries/wapi";

const { rows: infoRows, loading: infoLoading } = useFarmInfo();
const { rows: summaryRows, loading: summaryLoading } = useFarmBlocksSummary({ since: 10000 });

const { t } = useI18n();
const infoTiles = computed(() => [
    {
        field: "total_bytes",
        label: t("farmer_info.physical_size"),
        format: (item) => `${(item / Math.pow(1000, 4)).toFixed(3)} TB`,
    },
    {
        field: "total_bytes_effective",
        label: "Effective Size", // TODO i18n
        format: (item) => `${(item / Math.pow(1000, 4)).toFixed(3)} TBe`,
    },
]);

const summaryTiles = computed(() => [
    {
        field: "num_blocks",
        label: "No. Blocks", // TODO i18n
    },
    {
        field: "total_rewards_value",
        label: "Total Rewards", // TODO i18n
        format: (item) => `${(item ?? 0).toFixed(3)} MMX`,
    },
]);
</script>
