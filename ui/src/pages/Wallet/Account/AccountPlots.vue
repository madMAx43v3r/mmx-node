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

            <template v-slot:body-cell-actions="bcProps">
                <q-td :props="bcProps" class="q-gutter-x-xs">
                    <q-btn
                        :label="t('account_contract_summary.deposit')"
                        :_icon="mdiBankTransferIn"
                        color="positive"
                        outline
                        @click="handleDeposit(bcProps.row.address, bcProps.row.owner)"
                    />
                    <q-btn
                        :label="t('account_contract_summary.withdraw')"
                        :_icon="mdiBankTransferOut"
                        color="negative"
                        outline
                        @click="handleWithdraw(bcProps.row.address, bcProps.row.owner)"
                    />
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

        <div class="row justify-end">
            <q-btn
                :label="$t('account_plots.new_plot')"
                :icon="mdiReceiptTextPlus"
                text-color="primary"
                outline
                rounded
                :to="`/wallet/account/${props.index}/create/virtualplot`"
            />
        </div>
    </div>
</template>

<script setup>
import { mdiReceiptTextPlus, mdiBankTransferIn, mdiBankTransferOut } from "@mdi/js";
const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

const { t } = useI18n();
const columns = computed(() => [
    {
        label: t("account_plots.balance"),
        field: (rows) => rows.balance.value,
        xclasses: "main-currency",
        headerStyle: "width: 8%",
    },

    {
        label: t("account_plots.size"),
        field: "size_bytes",
        format: (item) => parseFloat((item / Math.pow(1000, 4)).toFixed(3)),
        xclasses: "physical-size",
        headerStyle: "width: 8%",
    },
    {
        name: "contract",
        label: t("account_plots.address"),
        field: "address",
        headerStyle: "width: 20%",
        align: "left",
    },
    {
        name: "actions",
        align: "right",
    },
]);

import { useWalletPlots } from "@/queries/wapi";
const { rows, loading } = useWalletPlots(props);

const $q = useQuasar();
const handleDeposit = (address, owner) => {
    $q.dialog({
        component: defineAsyncComponent(() => import("@/components/Dialogs/VirtualPlotDepositDialog")),
        componentProps: {
            index: props.index,
            owner,
            address,
        },
    });
};

const handleWithdraw = (address, owner) => {
    $q.dialog({
        component: defineAsyncComponent(() => import("@/components/Dialogs/VirtualPlotWithdrawDialog")),
        componentProps: {
            index: props.index,
            owner,
            address,
        },
    });
};
</script>

<style scoped>
.physical-size {
    font-weight: bold;
}
.physical-size:after {
    content: " TB";
    font-weight: normal;
}
</style>
