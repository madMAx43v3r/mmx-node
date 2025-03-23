<template>
    <div class="q-gutter-y-sm">
        <q-card flat>
            <q-card-section>
                <TokenSelect
                    v-model="token"
                    :label="$t('common.token')"
                    :clearable="token !== null"
                    null-item
                    :null-label="$t('market_menu.anything')"
                />

                <TokenSelect
                    v-model="currency"
                    :label="$t('common.currency')"
                    :clearable="currency !== null"
                    null-item
                    :null-label="$t('market_menu.anything')"
                />
            </q-card-section>
        </q-card>
    </div>
</template>

<script setup>
import { useSwapMenuStore } from "../useSwapMenuStore";
const { token, currency } = useSwapMenuStore();
const { activeWalletIndex } = useActiveWalletStore();
</script>
