<template>
    <!-- TODO filter -->

    <div v-for="(row, key) in filteredRows" :key="key" class="q-gutter-y-xs">
        <AccountContractView :data="row" />
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
