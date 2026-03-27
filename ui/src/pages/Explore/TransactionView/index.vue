<template>
    <div class="q-gutter-y-sm">
        <div>
            <div class="row q-gutter-x-none">
                <m-chip>{{ $t("transaction_view.transaction") }}</m-chip>
                <m-chip>{{ transactionId }}</m-chip>
            </div>
            <q-card flat>
                <EmptyState v-if="noData" :icon="mdiBank" :title="$t('transaction_view.no_such_transaction')" />
                <ListTable v-else :rows="rows" :data="data" :loading="loading" />
            </q-card>
        </div>
        <template v-if="data">
            <q-card flat>
                <InputOutputTable :rows="data.inputs" is-input :loading="loading" />
            </q-card>
            <q-card flat>
                <InputOutputTable :rows="data.outputs" :is-input="false" :loading="loading" />
            </q-card>

            <div v-for="(op, index) in data.operations" :key="index">
                <div class="row q-gutter-x-none">
                    <m-chip> Operation[{{ index }}] </m-chip>
                    <m-chip>{{ op.__type }}</m-chip>
                </div>
                <q-card flat>
                    <ObjectTable :data="op" />
                </q-card>
            </div>

            <div v-if="data.deployed">
                <div class="row q-gutter-x-none">
                    <m-chip>{{ data.deployed.__type }}</m-chip>
                </div>
                <q-card flat>
                    <ObjectTable :data="data.deployed" />
                </q-card>
            </div>
        </template>
    </div>
</template>
<script setup>
import { mdiBank } from "@mdi/js";
import InputOutputTable from "./InputOutputTable.vue";

const props = defineProps({
    transactionId: {
        type: String,
        required: true,
    },
});

const { t } = useI18n();

const rows = computed(() => [
    {
        label: t("transaction_view.height"),
        field: "height",
        to: (data) => (data.height != null ? `/explore/block/height/${data.height}` : null),
        format: (height) => (height == null ? t("common.pending") : height),
        classes: (data) => (data.height == null ? "text-italic" : ""),
    },
    {
        label: t("transaction_view.message"),
        field: "message",
        visible: (data) => data.did_fail,
    },
    {
        label: t("transaction_view.confirmed"),
        field: "confirm",
        visible: (data) => data.confirm,
    },
    {
        label: t("transaction_view.expires"),
        field: "expires",
        visible: (data) => data.expires < 4294967295,
    },

    {
        label: t("transaction_view.note"),
        field: "note",
    },
    {
        label: t("transaction_view.time"),
        field: "time_stamp",
        format: (item) => new Date(item).toLocaleString(),
        visible: (data) => data.time,
    },
    {
        label: t("transaction_view.address"),
        field: "address",
        to: (data) => `/explore/address/${data.address}`,
        visible: (data) => data.deployed,
        classes: "mono",
    },
    {
        label: t("transaction_view.sender"),
        field: "sender",
        to: (data) => `/explore/address/${data.sender}`,
        visible: (data) => data.sender,
        classes: "mono",
    },
    { label: t("transaction_view.cost"), field: (data) => data.cost.value, classes: "main-currency" },
    { label: t("transaction_view.fee"), field: (data) => data.fee.value, classes: "main-currency" },
]);

import { useTransaction } from "@/queries/wapi";
const { data, loading, noData } = useTransaction(reactive({ id: toRef(() => props.transactionId) }));
</script>
