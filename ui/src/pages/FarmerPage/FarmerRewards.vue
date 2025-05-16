<template>
    <div class="q-gutter-y-sm">
        <ListTable :rows="rows" :data="data" :loading="loading" />
        <q-table
            :rows="data?.rewards ?? []"
            :columns="columns"
            :loading="loading"
            :pagination="{ rowsPerPage: 0 }"
            hide-pagination
            flat
        >
            <template v-slot:body-cell-address="bcProps">
                <q-td :props="bcProps">
                    <RouterLink :to="`/explore/address/${bcProps.value}`" class="text-primary mono">
                        {{ bcProps.value }}
                    </RouterLink>
                </q-td>
            </template>
        </q-table>
    </div>
</template>

<script setup>
const { t } = useI18n();
const rows = computed(() => [
    {
        label: "No. Blocks", // TODO i18n
        field: "num_blocks",
    },
    {
        label: "Last Height", // TODO i18n
        field: "last_height",
        format: (last_height) => (last_height > 0 ? last_height : "N/A"),
    },
    {
        label: "Total Rewards", // TODO i18n
        field: "total_rewards_value",
        classes: "main-currency",
    },
    {
        label: "Time to win", // TODO i18n
        field: () => estTimeToWin,
        format: (item) => `${item.value} (on average)`,
        loading: estTimeToWinLoading.value,
    },
]);

const columns = computed(() => [
    {
        label: "Reward", //TODO i18n
        field: (row) => row.value,
        headerStyle: "width: 5%",
    },
    {
        field: (row) => "MMX",
        headerStyle: "width: 5%",
    },
    {
        name: "address",
        label: t("transaction_view.address"),
        field: (row) => row.address,
        align: "left",
    },
]);

import { useFarmBlocksSummary, useNodeInfo, useFarmInfo } from "@/queries/wapi";
const { data, loading } = useFarmBlocksSummary({ since: 10000 });
const { rows: nodeInfoRows, loading: nodeInfoLoading } = useNodeInfo();
const { rows: farmerInfoRows, loading: farmerInfoLoading } = useFarmInfo();

const estTimeToWinLoading = computed(() => loading.value || nodeInfoLoading.value || farmerInfoLoading.value);
const netspace = computed(() => nodeInfoRows.value.total_space); // [GB]
const farmSize = computed(() => farmerInfoRows.value.total_bytes_effective / 1e9); // [GB]

const estTimeToWin = computed(() => {
    if (netspace.value && farmSize.value) {
        const estMin = netspace.value / farmSize.value / 6;
        if (estMin <= 60) {
            return Math.floor(estMin) + " minutes";
        }
        const estHours = estMin / 60;
        if (estHours <= 24) {
            return Math.floor(estHours) + " hours and " + Math.floor(estMin % 60) + " minutes";
        }
        const est_days = estHours / 24;
        return Math.floor(est_days) + " days and " + Math.floor(estHours % 24) + " hours";
    }
    return "N/A";
});
</script>
