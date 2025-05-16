<template>
    <div>
        <q-linear-progress v-if="loading" query class="absolute-top" />
        <div class="q-gutter-y-md">
            <template v-for="(account, key) in rows" :key="key">
                <AccountView :index="account.account" :use-router-view="false" />
            </template>

            <div class="row justify-end">
                <q-btn
                    to="/wallet/create"
                    :icon="mdiWalletPlus"
                    :label="$t('wallet_summary.new_wallet')"
                    size="md"
                    outline
                    rounded
                    color="primary"
                />
            </div>
        </div>
    </div>
</template>

<script setup>
import { mdiWalletPlus } from "@mdi/js";
import AccountView from "@/pages/Wallet/AccountView";

import { useWalletAccounts } from "@/queries/wapi";
const { rows, loading } = useWalletAccounts();
</script>
