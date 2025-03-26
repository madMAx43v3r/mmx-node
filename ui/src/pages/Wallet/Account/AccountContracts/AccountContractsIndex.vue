<template>
    <div>
        <template v-for="(chip, key) in filterChips" :key="key">
            <q-chip v-model:selected="filter[chip.address]" outline color="primary" text-color="white">
                {{ chip.name }}
            </q-chip>
        </template>
        <div class="q-gutter-y-md">
            <template v-for="(row, key) in filteredRows" :key="key">
                <AccountContractView :data="row" />
            </template>
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

import { useWalletContracts } from "@/queries/wapi";
const { rows, loading } = useWalletContracts(props);

const { chainBinaries, chainBinariesSwapped, filterInit } = useChainBinaries();

const filter = ref({});
watchEffect(() => {
    filter.value = filterInit.value;
});

const filterChips = computed(() => {
    const distinctWalletContractBinaries = Array.from(
        new Set(rows.value.filter((row) => chainBinariesSwapped.value[row.binary]).map((row) => row.binary))
    );

    const result = Object.fromEntries(
        Object.entries(chainBinariesSwapped.value).filter(([key, value]) =>
            distinctWalletContractBinaries.includes(key)
        )
    );

    const hasOthers = Object.entries(rows.value).some(([key, value]) => typeof result[value.binary] === "undefined");

    if (hasOthers) {
        result.others = {
            name: "Others", // TODO i18n
            address: "others",
            key: "others",
        };
    }

    return result;
});

const filteredRows = computed(() =>
    rows.value.filter(
        (row) => filter.value[row.binary] || (filter.value.others && typeof filter.value[row.binary] === "undefined")
    )
);

const router = useRouter();
const handleDeposit = (index, address) => {
    router.push("/wallet/account/" + index + "/send/" + address);
};
const handleWithdraw = (index, address) => {
    router.push("/wallet/account/" + index + "/send_from/" + address);
};
</script>
