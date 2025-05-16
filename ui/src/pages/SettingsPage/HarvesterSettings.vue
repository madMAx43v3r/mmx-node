<template>
    <q-card flat>
        <q-card-section>
            <q-input
                label="Num. Threads (for lookups, reload)"
                :model-value="data.harv_num_threads"
                :rules="[rules.number]"
                @change="(value) => (data.harv_num_threads = parseInt(value))"
            />
            <q-input
                :label="$t('harvester_settings.harvester_reload_interval')"
                :model-value="data.reload_interval"
                :rules="[rules.number]"
                @change="(value) => (data.reload_interval = parseInt(value))"
            />
            <q-checkbox v-model="data.recursive_search" label="Recursive Search" />
            <q-markup-table flat>
                <thead>
                    <tr>
                        <th align="left">{{ $t("harvester_settings.plot_directory") }}</th>
                        <th width="20%"></th>
                    </tr>
                </thead>
                <tbody>
                    <tr v-for="item in data.plot_dirs" :key="item">
                        <td>{{ item }}</td>
                        <td align="right">
                            <q-btn label="Remove" outline @click="handleRemovePlotDir(item)" />
                        </td>
                    </tr>
                </tbody>
            </q-markup-table>
            <q-input v-model="newPlotDir" :label="$t('harvester_settings.plot_directory')" />
            <q-btn
                outlined
                :disable="!newPlotDir?.length"
                color="primary"
                class="q-mt-sm"
                @click="handleAddPlotDir(newPlotDir)"
            >
                {{ $t("harvester_settings.add_directory") }}
            </q-btn>
        </q-card-section>
    </q-card>
</template>

<script setup>
import { mdiTrashCan } from "@mdi/js";

import { useAddPlotDir, useRemovePlotDir } from "@/queries/api";
import rules from "@/helpers/rules";

const newPlotDir = ref();

const { data, loading } = useConfigData();

const addPlotDir = useAddPlotDir();
const handleAddPlotDir = (path) => {
    if (!path?.length) return;

    addPlotDir.mutate(path, {
        onSuccess: () => {
            newPlotDir.value = "";
        },
    });
};
const removePlotDir = useRemovePlotDir();
const handleRemovePlotDir = (path) => {
    removePlotDir.mutate(path);
};
</script>
