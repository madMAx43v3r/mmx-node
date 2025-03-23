<template>
    <q-table :rows="rows" :columns="columns" :loading="loading" :pagination="{ rowsPerPage: 0 }" hide-pagination flat>
        <template v-if="loading" v-slot:bottom-row="brProps">
            <TableBodySkeleton :props="brProps" :row-count="20" />
        </template>

        <template v-slot:body-cell-received="bcProps">
            <q-td :props="bcProps">
                <b>{{ bcProps.value }}</b> MB
            </q-td>
        </template>

        <template v-slot:body-cell-send="bcProps">
            <q-td :props="bcProps">
                <b>{{ bcProps.value }}</b> MB
            </q-td>
        </template>

        <template v-slot:body-cell-ping="bcProps">
            <q-td :props="bcProps">
                <b>{{ bcProps.value }}</b> ms
            </q-td>
        </template>

        <template v-slot:body-cell-duration="bcProps">
            <q-td :props="bcProps">
                <b>{{ bcProps.value }}</b> min
            </q-td>
        </template>

        <template v-slot:body-cell-action="bcProps">
            <q-td :props="bcProps">
                <q-btn
                    :label="$t('node_peers.kick')"
                    outline
                    color="secondary"
                    @click="handleKickPeer(bcProps.row.address)"
                />
            </q-td>
        </template>
    </q-table>
</template>

<script setup>
const { t } = useI18n();

const columns = computed(() => [
    {
        label: t("node_peers.ip"),
        field: "address",
        headerStyle: "width: 10%",
    },
    {
        label: t("node_peers.height"),
        field: "height",
        headerStyle: "width: 7%",
    },
    {
        label: t("node_peers.type"),
        field: "type",
        headerStyle: "width: 10%",
    },
    {
        label: t("node_peers.version"),
        field: "version",
        headerStyle: "width: 5%",
    },
    {
        name: "received",
        label: t("node_peers.received"),
        field: "bytes_recv",
        format: (item) => (item / 1024 / 1024).toFixed(1),
    },
    {
        name: "send",
        label: t("node_peers.send"),
        field: "bytes_send",
        format: (item) => (item / 1024 / 1024).toFixed(1),
    },
    { name: "ping", label: t("node_peers.ping"), field: "ping_ms" },
    {
        name: "duration",
        label: t("node_peers.duration"),
        field: "connect_time_ms",
        format: (item) => (item / 1000 / 60).toFixed(),
    },
    {
        label: t("node_peers.connection"),
        field: "is_outbound",
        format: (item) => (item ? t("node_peers.outbound") : t("node_peers.inbound")),
    },
    { name: "action", label: "Action", field: "action", align: "right" }, // TODO i18n
]);

import { usePeers, useKickPeer } from "@/queries/api";
const { rows, loading } = usePeers();

const kickPeer = useKickPeer();
const handleKickPeer = (address) => {
    kickPeer.mutate(address);
};
</script>
