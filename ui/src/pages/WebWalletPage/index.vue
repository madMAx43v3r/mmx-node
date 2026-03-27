<template>
    <q-page padding>
        <template v-if="!wallet">
            <SeedInputForm v-model:wallet="wallet" />
        </template>
        <template v-else>
            <transition appear enter-active-class="animate__animated animate__fadeIn">
                <div v-if="address" class="q-gutter-y-sm">
                    <div>
                        <div class="row q-col-gutter-xs">
                            <m-chip class="col-auto">
                                {{ $t("common.address") }}
                            </m-chip>
                            <m-chip copy class="col-auto">
                                {{ address }}
                            </m-chip>
                            <m-chip
                                square
                                outline
                                :ripple="false"
                                class="col-auto no-padding"
                                style="border-style: none !important"
                            >
                                <q-btn
                                    label="Logout"
                                    :icon="mdiLogout"
                                    color="secondary"
                                    size="sm"
                                    outline
                                    @click="handleLogout"
                                />
                            </m-chip>
                        </div>

                        <q-card flat>
                            <BalanceTable :address="address" show-empty />
                        </q-card>
                    </div>

                    <SendForm :wallet="wallet" :address="address" />
                    <AddressHistoryTable :address="address" :limit="20" show-empty />
                </div>
            </transition>

            <q-inner-loading :showing="!address">
                <q-spinner-cube size="50px" color="primary" />
            </q-inner-loading>
        </template>
    </q-page>
</template>

<script setup>
import { mdiLogout } from "@mdi/js";

import SendForm from "./SendForm";

const walletStore = useWalletStore();
const { wallet } = storeToRefs(walletStore);

const address = computedAsync(
    async () => {
        return wallet.value && (await wallet.value.getAddressAsync(0));
    },
    null,
    {
        onError: (error) => {
            throw error;
        },
    }
);

const handleLogout = () => {
    walletStore.doLogout();
};
</script>
