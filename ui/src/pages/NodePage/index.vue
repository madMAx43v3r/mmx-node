<template>
    <q-page padding>
        <div class="q-gutter-y-sm">
            <div class="q-col-gutter-xs">
                <NodeInfo />
                <FarmerInfo v-if="isFarmer" />
            </div>
            <q-card flat>
                <NodeMenu />
                <TrRouterView />
            </q-card>
        </div>
    </q-page>
</template>

<script setup>
import NodeInfo from "./NodeInfo";
import NodeMenu from "./NodeMenu";
import FarmerInfo from "../FarmerPage/FarmerInfo";

const { isFarmer } = useConfigData();
</script>
