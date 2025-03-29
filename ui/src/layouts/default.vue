<template>
    <!-- {{ $q.screen.name }} -->
    <q-layout view="lhr Lpr lFr">
        <q-header class="m-bg-grey">
            <div class="row justify-center">
                <div :class="layoutClass">
                    <component :is="MainMenu" class="q-mx-lg" />
                </div>
            </div>
        </q-header>

        <div class="row justify-center">
            <div :class="layoutClass">
                <q-page-container>
                    <TrRouterView />
                </q-page-container>
            </div>
        </div>
    </q-layout>
</template>

<script setup>
import MainMenu from "@/components/MainMenu";

const appStore = useAppStore();
const layoutClass = computed(() => ({
    "col-12": true,
    "app-max-width": !appStore.isWinGUI,
}));
</script>
