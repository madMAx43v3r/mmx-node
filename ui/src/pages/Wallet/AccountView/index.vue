<template>
    <template v-if="!noData">
        <div>
            <AccountHeader :index="index" />
            <AccountMenu :index="index" />
            <AccountBalanceTable v-if="!useRouterView" :index="index" class="q-mt-sm" />
            <TrRouterView :index="index" class="q-mt-sm" />
        </div>
    </template>
    <template v-else>
        <EmptyState title="No such wallet" :icon="mdiWallet" />
    </template>
</template>

<script setup>
import { mdiWallet } from "@mdi/js";

import AccountHeader from "./AccountHeader";
import AccountMenu from "./AccountMenu";

const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
    useRouterView: {
        type: Boolean,
        required: false,
        default: true,
    },
});

import { useWalletAccount } from "@/queries/wapi";
const { rows, loading, noData } = useWalletAccount(reactive({ index: props.index }));

const sessionStore = useSessionStore();
if (props.useRouterView) {
    sessionStore.activeWalletIndex = props.index;
}
</script>
