<template>
    <q-table
        :rows="rowsFiltered"
        :columns="columns"
        :loading="loading"
        :pagination="{ rowsPerPage: 0 }"
        hide-pagination
        flat
        dense
    >
        <template v-slot:top>
            <q-btn-toggle v-model="module" :options="modules" toggle-color="primary" color="grey-6" dense flat />
        </template>

        <template v-if="loading" v-slot:bottom-row="brProps">
            <TableBodySkeleton :props="brProps" :row-count="limit" />
        </template>

        <template v-slot:body-cell="bcProps">
            <q-td :props="bcProps" :class="getCellClass(bcProps)">
                <template v-if="bcProps.col.name == 'message'">
                    <template v-if="bcProps.value.includes('------------')">
                        <span class="flex">
                            <span style="font-size: 0px">{{ bcProps.value }}</span>
                            <q-separator color="orange" style="flex-grow: 1" class="q-mr-md" />
                        </span>
                    </template>
                    <template v-else>
                        <span
                            v-for="(chunk, key) in bcProps.value.split(numberRegExp)"
                            :key="key"
                            :set="m = bcProps.value.match(numberRegExp)"
                        >
                            <template v-if="m && m.includes(chunk)">
                                <span style="color: darkcyan">{{ chunk }}</span>
                            </template>
                            <template v-else>{{ chunk }}</template>
                        </span>
                    </template>
                </template>
                <template v-else>
                    {{ bcProps.value }}
                </template>
            </q-td>
        </template>
    </q-table>
</template>

<script setup>
//const numberRegExp = /(?<=height )(\d+(?:\.\d+)?)(?=,|\s|$)/g;
const numberRegExp = /(?<=\s|^)(\d+(?:\.\d+)?)(?=,|\s|$)/g;
const { t } = useI18n();
const appStore = useAppStore();

const getCellClass = (props) => {
    const level = props.row.level;
    if (level == 1) {
        return "error";
    }
    if (level == 2) {
        return "warning";
    }
};

const columns = computed(() => [
    {
        label: t("node_log_table.time"),
        field: "time",
        align: "left",
        headerStyle: "width: 6%",
        format: (item) => new Date(item / 1000).toLocaleTimeString(),
    },
    {
        label: t("node_log_table.module"),
        field: "module",
        align: "right",
        headerStyle: "width: 8%",
    },
    {
        name: "message",
        label: t("node_log_table.message"),
        field: "message",
        align: "left",
    },
]);

const modules = computed(() => [
    { value: null, label: t("node_log.terminal") },
    { value: "Node", label: t("node_log.node") },
    { value: "Router", label: t("node_log.router") },
    { value: "Wallet", label: t("node_log.wallet") },
    { value: "Farmer", label: t("node_log.farmer") },
    { value: "Harvester", label: t("node_log.harvester") },
    { value: "TimeLord", label: t("node_log.timelord") },
]);

const limit = ref(100);
const level = ref(3);
const module = ref(null);

import { useNodeLog } from "@/queries/wapi";
const { rows, loading } = useNodeLog(reactive({ limit, level, module }));

const rowsFiltered = computed(() =>
    Array.isArray(rows.value) ? rows.value.filter((item) => !appStore.isWinGUI || item.module != "HttpServer") : []
);
</script>

<style scoped>
::v-deep(.error) {
    background-color: rgb(198, 40, 40, 0.4);
}

::v-deep(.warning) {
    background-color: rgb(255, 143, 0, 0.4);
}

::v-deep(.q-table tbody tr td:nth-of-type(3)) {
    font-family: "Roboto Mono Variable", monospace;
    letter-spacing: -0.75px;
    text-wrap: wrap;
}

::v-deep(.q-btn-group .q-btn) {
    padding-left: 1em;
    padding-right: 1em;
}
</style>
