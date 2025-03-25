<template>
    <q-page padding>
        <div class="q-gutter-y-sm">
            <FarmerInfo />
            <FarmerRewards />
            <TrRouterView />
        </div>
    </q-page>
</template>

<script setup>
import FarmerInfo from "./FarmerInfo";
import FarmerRewards from "./FarmerRewards";
</script>
