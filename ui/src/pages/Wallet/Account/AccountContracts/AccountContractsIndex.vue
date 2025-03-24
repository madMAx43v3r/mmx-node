<template>
    <!-- TODO filter -->

    <div v-for="(row, key) in filteredRows" :key="key" class="q-gutter-y-xs">
        <div>
            <div class="row q-gutter-x-none">
                <m-chip>{{ row.__type }}</m-chip>
                <m-chip>{{ row.address }}</m-chip>
            </div>

            <q-card flat>
                <ObjectTable :data="row" />
            </q-card>
        </div>
        <div>
            <BalanceTable :address="row.address" />
        </div>
        <div v-if="row.__type != 'mmx.contract.Executable'" class="row justify-end q-gutter-xs">
            <q-btn
                :label="$t('account_contract_summary.deposit')"
                :icon="mdiBankTransferIn"
                outline
                color="positive"
                @click="handleDeposit(index, row.address)"
            />

            <q-btn
                :label="$t('account_contract_summary.withdraw')"
                :icon="mdiBankTransferOut"
                outline
                color="negative"
                @click="handleWithdraw(index, row.address)"
            />
        </div>
    </div>
</template>
<script setup>
import { mdiBankTransferIn, mdiBankTransferOut } from "@mdi/js";
const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

import { useWalletContracts } from "@/queries/wapi";
const { rows, loading } = useWalletContracts(props);

const filteredRows = computed(() => rows.value);

const router = useRouter();
const handleDeposit = (index, address) => {
    router.push("/wallet/account/" + index + "/send/" + address);
};
const handleWithdraw = (index, address) => {
    router.push("/wallet/account/" + index + "/send_from/" + address);
};
</script>
