<template>
    <q-table :rows="rows" :columns="columns" :loading="loading" :pagination="{ rowsPerPage: 0 }" hide-pagination flat>
        <!-- <template v-if="loading" v-slot:bottom-row="brProps">
            <TableBodySkeleton :props="brProps" :row-count="1" />
        </template> -->

        <template v-slot:header-cell-contract="hcProps">
            <q-th :props="hcProps">
                <div class="row vertical-middle justify-between">
                    <span class="self-center">{{ hcProps.col.label }}</span>
                </div>
            </q-th>
        </template>

        <template v-slot:body-cell-contract="bcProps">
            <q-td :props="bcProps">
                <RouterLink
                    v-if="!bcProps.row.is_native"
                    :to="`/explore/address/${bcProps.value}`"
                    class="text-primary mono"
                >
                    {{ bcProps.value }}
                </RouterLink>
            </q-td>
        </template>
    </q-table>
</template>

<script setup>
const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

const { t } = useI18n();
const columns = computed(() => [
    {
        label: t("account_balance.balance"),
        field: "total",
        format: (item) => formatAmount(item),
        headerStyle: "width: 7%",
    },
    {
        label: t("account_balance.reserved"),
        field: "reserved",
        headerStyle: "width: 7%",
    },
    {
        label: t("account_balance.spendable"),
        field: "spendable",
        format: (item) => formatAmount(item),
        style: "font-weight: bold;",
        headerStyle: "width: 7%",
    },
    {
        label: t("account_balance.token"),
        field: "symbol",
        headerStyle: "width: 7%",
        align: "left",
    },
    {
        name: "contract",
        label: t("account_balance.contract"),
        field: "contract",
        align: "left",
    },
]);

import { useWalletBalance } from "@/queries/wapi";
const { rows, loading } = useWalletBalance(reactive({ index: toRef(() => props.index) }), (data) => data.balances);
</script>
