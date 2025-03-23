<template>
    <q-card :disable="loading" flat>
        <q-linear-progress v-if="loading" query class="absolute-top" />

        <q-checkbox v-model="data.timelord" :label="$t('node_settings.enable_timelord')" />
        <q-checkbox v-model="data.open_port" :label="$t('node_settings.open_port')" />
        <q-checkbox v-model="data.allow_remote" label="Allow remote service access (for remote harvesters)" />

        <q-select
            v-model="opencl_device_select"
            :label="$t('node_settings.opencl_device')"
            :options="data.opencl_device_list"
            emit-value
            map-options
        />
    </q-card>
</template>

<script setup>
const { data, loading } = useConfigData();

const opencl_device_select = ref();
watchEffect(() => (opencl_device_select.value = data?.opencl_device_select));

watch(opencl_device_select, (value) => {
    data.opencl_device_select = value;
    data.opencl_platform = null;
    data.opencl_device = value >= 0 ? data.opencl_device_list_relidx[value].index : -1;
    data.opencl_device_name = value >= 0 ? data.opencl_device_list_relidx[value].name : null;
});
</script>

<style scoped>
.q-checkbox {
    display: flex;
    flex-wrap: wrap;
    flex-direction: row;
}
</style>
