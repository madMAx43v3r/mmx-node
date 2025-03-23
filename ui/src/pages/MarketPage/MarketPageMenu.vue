<template>
    <div class="q-gutter-y-sm">
        <q-card flat>
            <q-card-section>
                <WalletSelect v-model="activeWalletIndex" :label="$t('common.wallet')" />
            </q-card-section>
        </q-card>
        <q-card flat>
            <q-card-section>
                <TokenSelect
                    v-model="bid"
                    :label="$t('market_menu.they_offer')"
                    :clearable="bid !== null"
                    null-item
                    :null-label="$t('market_menu.anything')"
                    :emit-value="false"
                />

                <TokenSelect
                    v-model="ask"
                    :label="$t('market_menu.they_ask')"
                    :clearable="ask !== null"
                    null-item
                    :null-label="$t('market_menu.anything')"
                    :emit-value="false"
                />
            </q-card-section>
        </q-card>
    </div>
</template>

<script setup>
import { useOfferMenuStore } from "./useOfferMenuStore";
const { bid, ask } = useOfferMenuStore();
const { activeWalletIndex } = useActiveWalletStore();
</script>
