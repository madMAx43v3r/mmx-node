<template>
    <q-table
        :rows="data?.plot_dirs ?? []"
        :columns="columns"
        :loading="loading"
        :pagination="{ rowsPerPage: 0 }"
        hide-pagination
        flat
    >
    </q-table>
</template>

<script setup>
const { t } = useI18n();
const columns = computed(() => [
    { label: t("farmer_plot_dirs.path"), field: (row) => row, headerStyle: "width: 7%", align: "left" },
]);

import { useFarmInfo } from "@/queries/wapi";
const { data, loading } = useFarmInfo();
</script>
