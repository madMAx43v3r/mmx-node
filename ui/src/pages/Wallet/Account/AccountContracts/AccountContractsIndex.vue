<template>
    <div>
        <template v-for="(address, key) in uniqBinaries" :key="key">
            <q-chip v-model:selected="filter[address]" outline color="primary" text-color="white">
                {{ chainBinariesSwapped[address].name }}
            </q-chip>
        </template>

        <div v-for="(row, key) in filteredRows" :key="key" class1="q-gutter-y-xs">
            <AccountContractView :data="row" />
        </div>
    </div>
</template>
<script setup>
import AccountContractView from "./AccountContractView";
import { mdiBankTransferIn, mdiBankTransferOut } from "@mdi/js";

const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

const filter = ref({});

import { useWalletContracts } from "@/queries/wapi";
const { rows, loading } = useWalletContracts(props);

const uniqBinaries = computed(() => Array.from(new Set(rows.value.map((row) => row.binary))));

const { chainBinariesSwapped, filterInit } = useChainBinaries();

watchEffect(() => {
    filter.value = filterInit.value;
});

const filteredRows = computed(() => rows.value.filter((row) => filter.value[row.binary]));

const router = useRouter();
const handleDeposit = (index, address) => {
    router.push("/wallet/account/" + index + "/send/" + address);
};
const handleWithdraw = (index, address) => {
    router.push("/wallet/account/" + index + "/send_from/" + address);
};
</script>
