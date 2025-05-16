<template>
    <q-page padding>
        <div class="q-gutter-y-sm">
            <MarketPageMenu />

            <q-card flat>
                <q-tabs align="left" indicator-color="primary" class="m-bg-grey">
                    <q-route-tab to="/market/offers" :label="$t('market_menu.offers')" />
                    <q-route-tab to="/market/history" :label="$t('market_menu.history')" />
                </q-tabs>
                <TrRouterView />
            </q-card>
        </div>
    </q-page>
</template>

<script setup>
import MarketPageMenu from "./MarketPageMenu.vue";
</script>
