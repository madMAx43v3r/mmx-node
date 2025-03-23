<template>
    <div class="q-gutter-y-sm">
        <q-table
            :rows="rows"
            :columns="columns"
            :loading="loading"
            :pagination="{ rowsPerPage: 0 }"
            hide-pagination
            flat
        >
            <template v-slot:body-cell-height="bcProps">
                <q-td :props="bcProps">
                    <RouterLink :to="`/explore/block/height/${bcProps.value}`" class="text-primary">
                        {{ bcProps.value }}
                    </RouterLink>
                </q-td>
            </template>

            <template v-slot:body-cell="bcProps">
                <q-td :props="bcProps">
                    <div :class="getXClasses(bcProps)">
                        {{ bcProps.value }}
                    </div>
                </q-td>
            </template>
        </q-table>
    </div>
</template>

<script setup>
const props = defineProps({
    limit: {
        type: Number,
        default: 50,
        required: false,
    },
});

const { t } = useI18n();
const columns = computed(() => [
    {
        name: "height",
        label: t("farmer_blocks.height"),
        field: "height",
        headerStyle: "width: 7%",
    },
    {
        label: "TX",
        field: "tx_count",
        headerStyle: "width: 7%",
    },
    {
        label: "K",
        field: "ksize",
        headerStyle: "width: 7%",
    },
    {
        label: t("farmer_blocks.score"),
        field: "score",
        headerStyle: "width: 7%",
    },
    {
        label: t("farmer_blocks.reward"),
        field: (data) => data.reward_amount.value,
        headerStyle: "width: 7%",
        xclasses: "main-currency",
    },
    {
        label: t("farmer_blocks.tx_fees"),
        field: (data) => data.tx_fees.value,
        headerStyle: "width: 7%",
        xclasses: "main-currency",
    },
    {
        label: t("farmer_blocks.time"),
        field: "time_stamp",
        format: (item) => new Date(item).toLocaleString(),
        align: "left",
    },
]);

import { useFarmBlocks } from "@/queries/wapi";
const { rows, loading } = useFarmBlocks(props);
</script>
