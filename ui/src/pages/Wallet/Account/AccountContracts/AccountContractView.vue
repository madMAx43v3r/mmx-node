<template>
    <div>
        <div class="row q-gutter-x-none">
            <m-chip>{{ data.__type }}</m-chip>
            <m-chip v-if="binary">
                {{ binary.name }}
            </m-chip>
            <m-chip>{{ data.address }}</m-chip>
        </div>

        <q-card flat>
            <ObjectTable :data="data" />
        </q-card>
    </div>
    <div>
        <BalanceTable :address="data.address" />
    </div>
    <div v-if="data.__type != 'mmx.contract.Executable'" class="row justify-end q-gutter-xs">
        <q-btn
            :label="$t('account_contract_summary.deposit')"
            :icon="mdiBankTransferIn"
            outline
            color="positive"
            @click="handleDeposit(index, data.address)"
        />

        <q-btn
            :label="$t('account_contract_summary.withdraw')"
            :icon="mdiBankTransferOut"
            outline
            color="negative"
            @click="handleWithdraw(index, data.address)"
        />
    </div>
</template>

<script setup>
const props = defineProps({
    data: {
        type: Object,
        required: true,
    },
});

const { chainBinariesSwapped } = useChainBinaries();
const binary = computed(() => chainBinariesSwapped.value[props.data?.binary]);
</script>
