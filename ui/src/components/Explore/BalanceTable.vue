<template>
    <q-table
        v-if="loading || (!loading && (rows?.length || showEmpty))"
        :rows="rows"
        :columns="columns"
        :loading="loading"
        :pagination="{ rowsPerPage: 0 }"
        hide-pagination
        flat
    >
        <template v-if="loading" v-slot:bottom-row="brProps">
            <TableBodySkeleton :props="brProps" :row-count="1" />
        </template>

        <template v-slot:body-cell-contract="bcProps">
            <q-td :props="bcProps">
                <RouterLink :to="`/explore/address/${bcProps.value}`" class="text-primary mono">
                    {{ bcProps.row.is_native ? "" : bcProps.value }}
                </RouterLink>
            </q-td>
        </template>
    </q-table>
</template>

<script setup>
const props = defineProps({
    address: {
        type: String,
        required: true,
    },
    showEmpty: {
        type: Boolean,
        default: false,
        required: false,
    },
});

const { t } = useI18n();

const columns = computed(() => {
    return [
        {
            label: t("balance_table.balance"),
            field: "total",
            format: (item) => formatAmount(item),
            headerStyle: "width: 7%",
        },
        {
            label: t("balance_table.locked"),
            field: "locked",
            format: (item) => formatAmount(item),
            headerStyle: "width: 7%",
        },
        {
            label: t("balance_table.spendable"),
            field: "spendable",
            format: (item) => formatAmount(item),
            headerStyle: "width: 7%",
            style: "font-weight: bold;",
        },
        {
            label: t("balance_table.token"),
            field: "symbol",
            headerStyle: "width: 7%",
            align: "left",
        },
        {
            name: "contract",
            label: t("balance_table.contract"),
            field: "contract",
            align: "left",
        },
    ];
});

import { useBalance } from "@/queries/wapi";
const { rows, loading } = useBalance(reactive({ id: toRef(() => props.address) }));
</script>
