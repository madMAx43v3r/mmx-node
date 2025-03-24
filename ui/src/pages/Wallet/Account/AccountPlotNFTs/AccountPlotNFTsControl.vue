<template>
    <div>
        <div class="row q-gutter-x-none">
            <m-chip>{{ data?.name }}</m-chip>
        </div>

        <q-card flat>
            <ListTable :rows="rows" :data="data" :loading="loading" />
        </q-card>
    </div>
</template>

<script setup>
const props = defineProps({
    address: {
        type: String,
        required: true,
    },
});

import { usePlotnft } from "@/queries/wapi";
const { data, loading } = usePlotnft(reactive({ id: props.address }));

const rows = computed(() => [
    {
        label: "Address", //TODO i18n
        field: (data) => data.address,
        to: (data) => `/explore/address/${data.address}`,
        classes: "mono",
        copyToClipboard: true,
    },
    {
        label: "Pool Server", //TODO i18n
        field: (data) => data.server_url ?? "N/A (solo farming)",
    },
    {
        label: "Unlock Height", //TODO i18n
        field: (data) => data.unlock_height,
        visible: (data) => data.is_locked,
    },
]);
</script>
