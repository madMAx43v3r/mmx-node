<template>
    <q-page padding>
        <div class="q-gutter-y-sm">
            <q-card flat>
                <q-card-section>
                    <WalletSelect v-model="activeWalletIndex" :label="$t('common.wallet')" />
                </q-card-section>
            </q-card>
            <TrRouterView />
        </div>
    </q-page>
</template>

<script setup>
import { useSwapMenuStore } from "../Swap/useSwapMenuStore";
const { token, currency } = useSwapMenuStore();
const { activeWalletIndex } = useActiveWalletStore();
</script>
