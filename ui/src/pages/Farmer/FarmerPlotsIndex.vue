<template>
    <div class="q-gutter-y-sm">
        <div class="row q-gutter-x-xs">
            <q-table
                :rows="rows?.plot_count ?? []"
                :columns="columns"
                :loading="loading"
                :pagination="{ rowsPerPage: 0 }"
                hide-pagination
                flat
                class="col-4"
            >
            </q-table>

            <q-table
                :rows="rows?.harvester_bytes ?? []"
                :columns="columns2"
                :loading="loading"
                :pagination="{ rowsPerPage: 0 }"
                hide-pagination
                flat
                class="col"
            >
                <template v-slot:body-cell="bcProps">
                    <q-td :props="bcProps">
                        <div :class="getXClasses(bcProps)">
                            {{ bcProps.value }}
                        </div>
                    </q-td>
                </template>
            </q-table>
        </div>

        <FarmerPlotDirs />

        <div class="row justify-end">
            <q-btn
                :label="$t('farmer_plots.reload_plots')"
                :icon="mdiRefresh"
                size="md"
                rounded
                outline
                color="primary"
                @click="handleHarvesterReload"
            />
        </div>
    </div>
</template>

<script setup>
import { mdiRefresh } from "@mdi/js";

const { t } = useI18n();
const columns = computed(() => [
    {
        label: t("farmer_plots.type"),
        field: "ksize",
        format: (item) => `K${item}`,
        headerStyle: "width: 20%",
    },
    {
        label: t("farmer_plots.count"),
        field: "count",
        align: "left",
        style: "font-weight: bold;",
    },
]);

const columns2 = computed(() => [
    { label: t("common.harvester"), field: "name", headerStyle: "width: 12%" },
    {
        label: t("farmer_info.physical_size"),
        field: "bytes",
        format: (item) => (item / Math.pow(1000, 4)).toFixed(3),
        xclasses: "physical-size",
        headerStyle: "width: 20%",
    },
    {
        label: "Effective Size", //TODO i18n
        field: "effective",
        format: (item) => (item / Math.pow(1000, 4)).toFixed(3),
        xclasses: "effective-size",
        align: "left",
    },
]);

const select = (data) => {
    var res = { plot_count: [], harvester_bytes: [] };
    for (const entry of data.plot_count) {
        res.plot_count.push({ ksize: entry[0], count: entry[1] });
    }
    for (const entry of data.harvester_bytes) {
        res.harvester_bytes.push({ name: entry[0], bytes: entry[1][0], effective: entry[1][1] });
    }
    return res;
};

import { useFarmInfo } from "@/queries/wapi";
const { rows, loading } = useFarmInfo(select);

import { useHarvesterReload } from "@/queries/api";
import FarmerPlotDirs from "./FarmerPlotDirs.vue";
const harvesterReload = useHarvesterReload();
const handleHarvesterReload = () => {
    harvesterReload.mutate();
};
</script>

<style scoped>
.physical-size {
    font-weight: bold;
}
.physical-size:after {
    content: " TB";
    font-weight: normal;
}

.effective-size {
    font-weight: bold;
}
.effective-size:after {
    content: " TBe";
    font-weight: normal;
}
</style>
