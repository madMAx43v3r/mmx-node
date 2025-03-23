<template>
    <q-page padding>
        <div class="q-gutter-y-sm">
            <FarmerInfo />
            <FarmerRewards />
            <div>
                <FarmerMenu />
                <TrRouterView />
            </div>
        </div>
    </q-page>
</template>

<script setup>
import FarmerInfo from "./FarmerInfo";
import FarmerRewards from "./FarmerRewards";
import FarmerMenu from "./FarmerMenu";
</script>
