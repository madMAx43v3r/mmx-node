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

            <template v-slot:body-cell-contract="bcProps">
                <q-td :props="bcProps">
                    <RouterLink :to="`/explore/address/${bcProps.value}`" class="text-primary mono">
                        {{ bcProps.value }}
                    </RouterLink>
                </q-td>
            </template>
        </q-table>
    </div>
</template>

<script setup>
const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

const { t } = useI18n();
const columns = computed(() => [{ label: "NFT", field: "item", align: "left" }]);

import { useWalletBalance } from "@/queries/wapi";
const { rows, loading } = useWalletBalance(
    reactive({ index: toRef(() => props.index), show_all: false }),
    (data) => data.nfts
);
</script>
