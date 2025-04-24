<template>
    <q-table :rows="rows" :columns="columns" :loading="loading" :pagination="{ rowsPerPage: 0 }" hide-pagination flat>
        <template v-if="loading" v-slot:bottom-row="brProps">
            <TableBodySkeleton :props="brProps" :row-count="props.limit" />
        </template>

        <template v-slot:body-cell-height="bcProps">
            <q-td :props="bcProps">
                <RouterLink :to="`/explore/block/height/${bcProps.value}`" class="text-primary">
                    {{ bcProps.value }}
                </RouterLink>
            </q-td>
        </template>

        <template v-slot:body-cell-hash="bcProps">
            <q-td :props="bcProps">
                <RouterLink :to="`/explore/block/hash/${bcProps.value}`" class="text-primary mono">
                    {{ getShortHash(bcProps.value, 16) }}
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
</template>

<script setup>
const props = defineProps({
    rows: {
        type: Object,
        required: true,
    },
    loading: {
        type: Boolean,
        required: false,
        default: false,
    },
});

const { t } = useI18n();
const columns = computed(() => [
    {
        name: "height",
        label: t("explore_blocks.height"),
        field: "height",
        headerStyle: "width: 7%",
    },
    {
        label: "VDF",
        field: "vdf_count",
        format: (item) => `+${item}`,
        headerStyle: "width: 5%",
    },
    {
        label: t("explore_blocks.tx"),
        field: "tx_count",
        headerStyle: "width: 5%",
    },
    {
        label: t("explore_blocks.k"),
        field: (row) => row.ksize,
        format: (item) => `k${item}`,
        headerStyle: "width: 5%",
    },
    {
        label: t("explore_blocks.score"),
        field: (row) => row.score,
        headerStyle: "width: 7%",
    },
    {
        label: t("explore_blocks.reward"),
        field: (row) => row.reward_amount.value,
        format: (item) => item.toFixed(3),
        xclasses: "main-currency",
        headerStyle: "width: 12%",
    },
    {
        label: "TX Fees", //TODO 1i8n
        field: (row) => (row.tx_count ? row.tx_fees.value : null),
        format: (item) => (item ? item.toFixed(3) : ""),
        xclasses: (item) => (item ? "main-currency" : item),
        headerStyle: "width: 12%",
    },
    {
        label: t("explore_blocks.size"),
        field: "static_cost_ratio",
        format: (item) => (item * 100).toFixed(1),
        xclasses: "percents",
        headerStyle: "width: 7%",
    },
    {
        label: t("explore_blocks.cost"),
        field: "total_cost_ratio",
        format: (item) => (item * 100).toFixed(1),
        xclasses: "percents",
        headerStyle: "width: 7%",
    },
    {
        name: "hash",
        label: t("explore_blocks.hash"),
        field: "hash",
        align: "left",
    },
]);
</script>
