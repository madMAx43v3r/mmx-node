<template>
    <AppLayout>
        <TrRouterView />
    </AppLayout>
    <VueQueryDevtools />
</template>

<script setup>
import { VueQueryDevtools } from "@tanstack/vue-query-devtools";

useWatchTheme();
useWatchI18n();
</script>
