<template>
    <div class="q-gutter-y-sm">
        <div>
            <div class="row q-gutter-x-none">
                <m-chip>Farmer</m-chip>
                <m-chip>{{ farmerKey }}</m-chip>
            </div>
            <q-card flat>
                <ListTable :rows="framerDataListRows" :data="framerData" :loading="framerDataLoading" />
            </q-card>
        </div>
        <q-card flat>
            <q-table
                :rows="framerData ? framerData.rewards : []"
                :columns="columns"
                :loading="framerDataLoading"
                :pagination="{ rowsPerPage: 0 }"
                hide-pagination
                flat
            >
                <template v-if="framerDataLoading" v-slot:bottom-row="brProps">
                    <TableBodySkeleton :props="brProps" :row-count="1" />
                </template>

                <template v-slot:body-cell-address="bcProps">
                    <q-td :props="bcProps">
                        <RouterLink :to="`/explore/address/${bcProps.value}`" class="text-primary mono">
                            {{ bcProps.value }}
                        </RouterLink>
                    </q-td>
                </template>
            </q-table>
        </q-card>
        <q-card flat>
            <BlockTable :rows="farmerBlocks" :loading="farmerBlocksLoading" class="q-py-sm" />
        </q-card>
    </div>
</template>

<script setup>
const props = defineProps({
    farmerKey: {
        type: String,
        required: true,
    },
    limit: {
        type: Number,
        required: false,
        default: 100,
    },
});

const { t } = useI18n();

const framerDataListRows = computed(() => [
    {
        label: "No. Blocks", //TODO i18n
        field: "block_count",
    },
    {
        label: "Total Rewards", //TODO i18n
        field: "total_reward_value",
        classes: "main-currency",
    },
]);

const columns = computed(() => [
    {
        label: "Reward", //TODO i18n
        field: "value",
        headerClasses: "key-cell",
    },
    {
        label: "",
        field: () => "MMX",
        headerStyle: "width: 7%",
        align: "left",
    },
    {
        name: "address",
        label: t("transaction_view.address"),
        field: "address",
        align: "left",
    },
]);

import { useFarmer, useFarmerBlocks } from "@/queries/wapi";
const { data: framerData, loading: framerDataLoading } = useFarmer(reactive({ id: toRef(() => props.farmerKey) }));
const { rows: farmerBlocks, loading: farmerBlocksLoading } = useFarmerBlocks(
    reactive({ id: toRef(() => props.farmerKey), limit: toRef(() => props.limit) })
);
</script>
