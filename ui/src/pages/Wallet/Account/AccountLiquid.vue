<template>
    <div class="q-gutter-y-sm">
        <q-table
            :rows="rows"
            :columns="columns"
            :loading="loading"
            :pagination="{ rowsPerPage: 0 }"
            hide-pagination
            flat
        >
            <template v-if="loading" v-slot:bottom-row="brProps">
                <TableBodySkeleton :props="brProps" :row-count="3" />
            </template>

            <template v-slot:body-cell-actions="bcProps">
                <q-td :props="bcProps" class="q-gutter-x-xs">
                    <q-btn
                        :label="t('common.manage')"
                        :_icon="mdiCashEdit"
                        outline
                        color="secondary"
                        :to="`/swap/${bcProps.row.address}/liquid`"
                    />
                </q-td>
            </template>

            <template v-slot:body-cell-symbol="bcProps">
                <q-td :props="bcProps">
                    <template v-if="bcProps.value != 'MMX'">
                        <RouterLink :to="`/explore/address/${bcProps.row.currency}`" class="text-primary">
                            {{ bcProps.value }}
                        </RouterLink>
                    </template>
                    <template v-else>
                        {{ bcProps.value }}
                    </template>
                </q-td>
            </template>

            <template v-slot:body-cell-contract="bcProps">
                <q-td :props="bcProps">
                    <RouterLink :to="`/explore/address/${bcProps.value}`" class="text-primary mono">
                        {{ bcProps.value }}
                    </RouterLink>
                </q-td>
            </template>

            <template v-slot:body-cell="bcProps">
                <q-td :props="bcProps">
                    <div :class="getXClasses(bcProps)">
                        {{ bcProps.value }}
                    </div>
                </q-td>
            </template>
        </q-table>
    </div>
</template>

<script setup>
import { mdiCashEdit } from "@mdi/js";
const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

const { t } = useI18n();
const columns = computed(() => [
    {
        label: t("common.balance"),
        field: (rows) => rows.balance.value,
        headerStyle: "width: 8%",
        align: "right",
    },
    {
        name: "symbol",
        label: t("common.symbol"),
        field: "symbol",
        headerStyle: "width: 6%",
        align: "left",
    },
    {
        name: "contract",
        label: t("common.address"),
        field: "address",
        headerStyle: "width: 70%",
        align: "left",
    },
    {
        name: "actions",
        align: "right",
    },
]);

import { useWalletSwapLiquid } from "@/queries/wapi";
const { rows, loading } = useWalletSwapLiquid(props);
</script>
