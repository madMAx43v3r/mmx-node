<template>
    <q-card flat>
        <q-table
            :rows="rows"
            :columns="columns"
            :loading="loading"
            :pagination="{ rowsPerPage: 0 }"
            hide-pagination
            flat
        >
            <template v-if="loading" v-slot:bottom-row="brProps">
                <TableBodySkeleton :props="brProps" :row-count="props.limit" />
            </template>

            <template v-slot:body-cell-farmer_key="bcProps">
                <q-td :props="bcProps">
                    <RouterLink :to="`/explore/farmer/${bcProps.value}`" class="text-primary mono">
                        {{ bcProps.value }}
                    </RouterLink>
                </q-td>
            </template>

            <template v-slot:body-cell-place="bcProps">
                <q-td :props="bcProps">
                    <template v-if="bcProps.rowIndex < 3">
                        <span>
                            <span class="prevent-select">{{ ["🥇", "🥈", "🥉"][bcProps.rowIndex] }}</span>
                            <span style="font-size: 0px">{{ bcProps.rowIndex + 1 }}</span>
                        </span>
                    </template>
                    <template v-else>{{ bcProps.rowIndex + 1 }}</template>
                </q-td>
            </template>
        </q-table>
    </q-card>
</template>

<script setup>
const props = defineProps({
    limit: {
        type: Number,
        default: 100,
    },
});

const { t } = useI18n();

const columns = computed(() => [
    {
        name: "place",
        headerStyle: "width: 5%",
        align: "center",
    },
    {
        label: t("explore_farmers.no_blocks"),
        field: "block_count",
        headerStyle: "width: 5%",
    },
    {
        name: "farmer_key",
        label: t("explore_farmers.farmer_key"),
        field: "farmer_key",
        align: "left",
    },
]);

import { useFarmers } from "@/queries/wapi";
const { rows, loading } = useFarmers(props);
</script>

<style scoped>
.main-currency:after {
    content: " MMX";
    font-weight: normal;
}

.prevent-select {
    -webkit-user-select: none; /* Safari */
    -ms-user-select: none; /* IE 10 and IE 11 */
    user-select: none; /* Standard syntax */
    display: inline-block;
}
</style>
