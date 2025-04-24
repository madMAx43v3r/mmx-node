<template>
    <div class="q-gutter-y-sm q-mt-sm">
        <ObjectTable :data="account" />
        <ObjectTable :data="keys" />
        <q-btn
            v-if="appStore.isWinGUI && keys"
            :label="$t('account_details.copy_keys_to_plotter')"
            color="primary"
            outline
            @click="handleCopyKeysToPlotter"
        />

        <q-table :rows="addresses" :columns="columns" :pagination="{ rowsPerPage: 0 }" hide-pagination flat>
            <template v-if="addressesLoading" v-slot:bottom-row="brProps">
                <TableBodySkeleton :props="brProps" :row-count="account.num_addresses ?? 1" />
            </template>

            <template v-slot:body-cell-index="bcProps">
                <q-td :props="bcProps">
                    {{ bcProps.rowIndex }}
                </q-td>
            </template>

            <template v-slot:body-cell-address="bcProps">
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

const appStore = useAppStore();

const columns = computed(() => [
    {
        name: "index",
        label: "Index", //TODO i18n
        field: (data) => data,
        headerClasses: "key-cell",
        classes: " m-bg-grey",
        align: "right",
    },
    {
        name: "address",
        label: "Address", //TODO i18n
        field: (data) => data,
        align: "left",
    },
]);

import { useWalletAccount, useWalletKeys, useWalletAddress } from "@/queries/wapi";
const { rows: account } = useWalletAccount(props, (data) => {
    delete data.is_hidden;
    return data;
});
const { rows: keys } = useWalletKeys(props);
const { rows: addresses, loading: addressesLoading } = useWalletAddress({ ...props, limit: 1000 });

const handleCopyKeysToPlotter = () => {
    window.mmx.copyKeysToPlotter(JSON.stringify(keys.value));
};
</script>
