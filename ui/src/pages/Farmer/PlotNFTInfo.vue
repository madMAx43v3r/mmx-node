<template>
    <div>
        <div>
            <div class="row q-gutter-x-none">
                <m-chip>{{ data.name }}</m-chip>
            </div>

            <q-card flat>
                <ListTable :rows="rows" :data="data" />
            </q-card>
        </div>
    </div>
</template>

<script setup>
const props = defineProps({
    address: {
        type: String,
        required: true,
    },
    data: {
        type: Object,
        required: true,
    },
});

const rows = computed(() => [
    {
        label: "Address", // TODO i18n
        field: (data) => props.address,
        to: (data) => `/explore/address/${data.address}`,
        classes: "mono",
        copyToClipboard: true,
    },
    {
        label: "Pool Server", // TODO i18n
        field: (data) => data.server_url ?? "N/A (solo farming)",
    },
    {
        label: "Pool Target", // TODO i18n
        field: (data) => data.pool_target,
        to: (data) => `/explore/address/${data.pool_target}`,
        classes: "mono",
        visible: (data) => data.pool_target,
    },
    {
        label: "Partial Difficulty", // TODO i18n
        field: (data) => data.partial_diff,
        visible: (data) => data.server_url,
    },
    {
        label: "Plot Count", // TODO i18n
        field: (data) => data.plot_count,
    },
]);
</script>
