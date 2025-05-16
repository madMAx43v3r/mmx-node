<template>
    <q-card flat style="background-color: transparent">
        <FarmerMenu />
        <div class="q-pt-sm q-gutter-y-sm">
            <template v-for="item in list" :key="item">
                <PlotNFTInfo :address="item[0]" :data="item[1]" />
            </template>

            <template v-if="list && !list.length">
                <q-banner rounded>
                    <template v-slot:avatar>
                        <q-icon :name="mdiAlert" color="warning" />
                    </template>
                    <!-- // TODO i18n -->
                    No NFT plots found
                </q-banner>

                <q-banner rounded>
                    <template v-slot:avatar>
                        <q-icon :name="mdiInformation" color="info" />
                    </template>
                    <!-- // TODO i18n -->
                    Need to create a PlotNFT in Wallet first to plot NFT plots
                </q-banner>
            </template>
        </div>
    </q-card>
</template>

<script setup>
import { mdiAlert, mdiInformation } from "@mdi/js";
import FarmerMenu from "../FarmerPage/FarmerMenu";

import PlotNFTInfo from "./PlotNFTInfo.vue";

import { useFarmInfo } from "@/queries/wapi";
const { data, loading } = useFarmInfo();

const list = computed(() => data.value?.pool_info ?? []);
</script>
