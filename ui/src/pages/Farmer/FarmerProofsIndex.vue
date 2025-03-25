<template>
    <q-card flat>
        <FarmerMenu />
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
        </q-table>
    </q-card>
</template>

<script setup>
import FarmerMenu from "../FarmerPage/FarmerMenu";

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
        label: "VDF height", //TODO i18n
        field: "vdf_height",
        headerStyle: "width: 7%",
    },
    {
        label: t("farmer_proofs.harvester"),
        field: "harvester",
        headerStyle: "width: 7%",
        align: "left",
    },
    {
        label: t("farmer_proofs.score"),
        field: (row) => row.proof.score,
        headerStyle: "width: 7%",
    },
    {
        label: t("farmer_proofs.sdiff"),
        field: (row) => row.proof.difficulty,
        headerStyle: "width: 7%",
    },
    {
        label: t("farmer_proofs.time"),
        field: "lookup_time_ms",
        headerStyle: "width: 7%",
        format: (item) => (item / 1000).toFixed(3),
    },
    { label: t("farmer_proofs.plot_id"), field: (row) => row.proof?.plot_id, align: "left", classes: "mono" },
]);

import { useFarmProofs } from "@/queries/wapi";
const { rows, loading } = useFarmProofs(props);
</script>
