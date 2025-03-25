<template>
    <q-card flat>
        <q-tabs align="left" indicator-color="primary" outside-arrows class="m-bg-grey">
            <template v-for="(item, key) in tabs" :key="key">
                <q-route-tab :to="item.to" :exact="item.exact ?? false" :label="item.label" :icon="item.icon">
                    <q-badge v-if="item.badge" color="positive" floating transparent rounded>
                        {{ item.badge }}
                    </q-badge>
                </q-route-tab>
            </template>
        </q-tabs>
    </q-card>
</template>

<script setup>
import { mdiCog } from "@mdi/js";

const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

const { t } = useI18n();
const tabs = computed(() => [
    { to: `/wallet/account/${props.index}`, exact: true, label: t("account_menu.balance") },
    // { to: `/wallet/account/${props.index}/nfts`, label: t("account_menu.nfts") },
    { to: `/wallet/account/${props.index}/contracts`, label: t("account_menu.contracts") },
    { to: `/wallet/account/${props.index}/send`, label: t("account_menu.send") },
    { to: `/wallet/account/${props.index}/history`, label: t("account_menu.history") },
    { to: `/wallet/account/${props.index}/log`, label: t("account_menu.log") },
    { to: `/wallet/account/${props.index}/offer`, label: t("account_menu.offer"), badge: offersWithAskBalance.value },
    { to: `/wallet/account/${props.index}/liquid`, label: t("account_menu.liquidity") },
    { to: `/wallet/account/${props.index}/plotnfts`, label: "PlotNFTs" },
    { to: `/wallet/account/${props.index}/details`, label: t("account_menu.info") },
    { to: `/wallet/account/${props.index}/options`, icon: mdiCog },
]);

import { useWalletOffers } from "@/queries/wapi";
const { rows: offers } = useWalletOffers(reactive({ index: toRef(() => props.index), state: true }));
const offersWithAskBalance = computed(() =>
    offers.value.reduce((total, item) => (item.ask_balance > 0 ? 1 : 0) + total, 0)
);
</script>
