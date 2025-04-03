<template>
    <div class="q-gutter-y-sm">
        <div>
            <div class="row q-gutter-x-none">
                <m-chip>{{ $t("block_view.block") }}</m-chip>
                <m-chip>{{ height }}</m-chip>
                <m-chip v-if="!noData">{{ data?.hash }}</m-chip>
                <q-space />
                <q-btn-group rounded>
                    <q-btn :to="prevPath" :icon="mdiArrowLeft" outline :disable="height <= 0" />
                    <q-btn :to="nextPath" :icon-right="mdiArrowRight" outline :disable1="noData" />
                </q-btn-group>
            </div>
            <q-card flat>
                <EmptyState v-if="noData" :icon="mdiDatabase" :title="$t('block_view.no_such_block')" />
                <ListTable v-else :rows="rows" :data="data" :loading="loading">
                    <template v-slot:value-vdf_reward_addr="{ item }">
                        <div v-for="(addr, index) in item.value" :key="addr">
                            <span>[{{ index }}]&nbsp;</span>
                            <RouterLink :to="`/explore/address/${addr}`" class="text-primary mono">
                                {{ addr }}
                            </RouterLink>
                        </div>
                    </template>
                </ListTable>
            </q-card>
        </div>

        <q-table
            v-if="data && data.tx_list.length"
            :rows="data.tx_list"
            :columns="tx_list_columns"
            :pagination="{ rowsPerPage: 0 }"
            hide-pagination
            flat
        >
            <template v-slot:body-cell-index="bcProps">
                <q-td :props="bcProps"> TX[{{ bcProps.rowIndex }}] </q-td>
            </template>

            <template v-slot:body-cell-id="bcProps">
                <q-td :props="bcProps">
                    <RouterLink :to="`/explore/transaction/${bcProps.value}`" class="text-primary mono">
                        {{ bcProps.value }}
                    </RouterLink>
                </q-td>
            </template>
        </q-table>
    </div>
</template>

<script setup>
import { mdiArrowLeft, mdiArrowRight, mdiDatabase } from "@mdi/js";

const props = defineProps({
    // eslint-disable-next-line vue/require-default-prop
    hash: {
        type: String,
        required: false,
    },
    // eslint-disable-next-line vue/require-default-prop
    height: {
        type: Number,
        required: false,
    },
});

const { t } = useI18n();

const rows = computed(() => [
    {
        label: t("block_view.height"),
        field: "height",
    },
    {
        label: t("block_view.hash"),
        field: "hash",
        classes: "mono",
    },
    {
        label: t("block_view.previous"),
        field: "prev",
        classes: "mono",
    },
    {
        label: t("block_view.time"),
        field: "time_stamp",
        format: (item) => new Date(item).toLocaleString(),
    },
    {
        label: "Reward Address", //TODO i18n
        field: (data) => data.reward_addr,
        to: (data) => `/explore/address/${data.reward_addr}`,
        classes: "mono",
        visible: (data) => data.reward_addr,
    },
    {
        label: "Reward Contract", //TODO i18n
        field: (data) => data.reward_contract,
        to: (data) => `/explore/address/${data.reward_contract}`,
        classes: "mono",
        visible: (data) => data.reward_contract,
    },
    {
        label: "Farmer Address", //TODO i18n
        field: (data) => data.reward_account,
        to: (data) => `/explore/address/${data.reward_account}`,
        classes: "mono",
        visible: (data) => data.reward_account,
    },
    {
        label: "Block Reward", //TODO i18n
        field: (data) => data.reward_amount.value,
        classes: "main-currency",
    },
    {
        label: "TX Fees", //TODO i18n
        field: (data) => data.tx_fees.value,
        classes: "main-currency",
    },
    {
        label: t("block_view.tx_count"),
        field: (data) => data.tx_count,
    },
    {
        label: t("block_view.time_diff"),
        field: "time_diff",
    },
    {
        label: t("block_view.space_diff"),
        field: (data) => data,
        format: (data) => `${data.space_diff}${data.is_space_fork ? "(updated)" : ""}`,
    },

    {
        label: "Reward Vote", //TODO i18n
        field: "reward_vote",
    },
    {
        label: "Reward Vote Sum", //TODO i18n
        field: (data) => data,
        format: (data) =>
            `${data.reward_vote_sum} / ${data.reward_vote_count} (${((data.reward_vote_count / ((data.height % 8640) + 1)) * 100).toFixed(1)} %)`,
    },
    {
        label: "Base Reward", //TODO i18n
        field: (data) => data.base_reward.value,
        classes: "main-currency",
    },
    {
        label: "Avg. TX Fee", //TODO i18n
        field: (data) => data.average_txfee.value,
        classes: "main-currency",
    },

    {
        label: "VDF Height", //TODO i18n
        field: "vdf_height",
    },
    {
        label: "VDF Count", //TODO i18n
        field: "vdf_count",
    },
    {
        label: t("block_view.vdf_iterations"),
        field: "vdf_iters",
    },
    {
        name: "vdf_reward_addr",
        label: "Timelord", //TODO i18n
        field: (data) => data.vdf_reward_addr,
    },
    {
        label: "Timelord Reward", //TODO i18n
        field: "vdf_reward_payout",
        to: (data) => `/explore/address/${data.vdf_reward_payout}`,
        visible: (data) => data.vdf_reward_payout,
        classes: "mono",
    },

    {
        label: t("block_view.k_size"),
        field: (data) => data.ksize,
    },
    {
        label: t("block_view.proof_score"),
        field: (data) => data.score,
    },

    {
        label: "No. Proofs", //TODO i18n
        field: (data) => data.proof.length,
    },

    {
        label: "Space Fork", //TODO i18n
        field: (data) => data,
        format: (data) => `${data.space_fork_proofs} / ${data.space_fork_len}`,
    },

    {
        label: t("block_view.plot_id"),
        field: (data) => data.proof[0].plot_id,
        visible: (data) => data.proof[0],
        classes: "mono",
    },
    {
        label: "Challenge", //TODO i18n
        field: (data) => data.proof[0].challenge,
        visible: (data) => data.proof[0],
    },

    {
        label: t("block_view.farmer_key"),
        field: (data) => data.farmer_key,
        to: (data) => `/explore/farmer/${data.farmer_key}`,
        classes: "mono",
    },
]);

const tx_list_columns = computed(() => [
    {
        name: "index",
        headerStyle: "width: 10%",
        headerClasses: "key-cell",
        classes: "key-cell m-bg-grey",
    },
    {
        label: t("block_view.inputs"),
        field: (row) => row.inputs.length + (row.exec_result ? row.exec_result.inputs.length : 0),
        headerStyle: "width: 7%",
    },
    {
        label: t("block_view.outputs"),
        field: (row) => row.outputs.length + (row.exec_result ? row.exec_result.outputs.length : 0),
        headerStyle: "width: 7%",
    },
    {
        label: t("block_view.operations"),
        field: (row) => row.execute.length,
        headerStyle: "width: 7%",
    },
    {
        name: "id",
        label: t("block_view.transaction_id"),
        field: (row) => row.id,
        align: "left",
    },
]);

import { useBlock } from "@/queries/wapi";
const { data, loading, noData } = useBlock(props);

const height = computed(() => props.height ?? data.value?.height ?? null);

const prevPath = computed(() => {
    if (data?.value) {
        return `/explore/block/hash/${data.value.prev}`;
    } else if (props.height != null) {
        return `/explore/block/height/${props.height - 1}`;
    }
    return null;
});

const nextPath = computed(() => {
    if (data?.value) {
        const height = data.value.height + 1;
        return `/explore/block/height/${height}`;
    } else if (props.height != null) {
        const height = props.height + 1;
        return `/explore/block/height/${height}`;
    }
    return null;
});

// const prefetch = usePrefetch();
// watchEffect(() => {
//     prefetch.path(prevPath.value);
//     prefetch.path(nextPath.value);
// });
</script>
