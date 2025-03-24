<template>
    <div>
        <AccountHeader :index="index" />
        <template v-if="!noData">
            <q-card flat>
                <AccountMenu :index="index" />
                <AccountBalanceTable v-if="!useRouterView" :index="index" />
            </q-card>
            <TrRouterView v-if="useRouterView" :index="index" />
        </template>
        <template v-else>
            <EmptyState title="No such wallet" :icon="mdiWallet" />
        </template>
    </div>
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
