<template>
    <div class="q-gutter-y-sm">
        <div>
            <div class="row q-gutter-x-none">
                <m-chip square outline>{{ $t("common.address") }}</m-chip>
                <m-chip square outline copy>{{ address }}</m-chip>
            </div>

            <q-card v-if="!noData" flat>
                <BalanceTable :address="address" show-empty />
            </q-card>
        </div>

        <div v-if="data">
            <m-chip square outline>{{ data.__type }}</m-chip>
            <q-card flat>
                <ObjectTable :data="data" />
            </q-card>
        </div>

        <q-card v-if="!noData" flat>
            <AddressHistoryTable :address="address" :limit="200" :show-empty="false" />
        </q-card>

        <EmptyState v-if="noData" :icon="mdiAccountQuestion" title="Invalid address" />
    </div>
</template>

<script setup>
import { mdiAccountQuestion } from "@mdi/js";
const props = defineProps({
    address: {
        type: String,
        required: true,
    },
});

import { useContract, useBalance } from "@/queries/wapi";
const { data, loading } = useContract(reactive({ id: toRef(() => props.address) }));
const { noData } = useBalance(reactive({ id: toRef(() => props.address) }));
</script>
