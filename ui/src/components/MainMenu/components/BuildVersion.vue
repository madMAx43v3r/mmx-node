<template>
    <template v-if="isNewVersionAvailable">
        <q-icon
            :name="mdiAlertDecagramOutline"
            color="warning"
            size="sm"
            class="animate__animated animate__flash animate__delay-1s"
        >
            <q-tooltip>This build is {{ version }}, compared to {{ latestVersion }} at madMAx43v3r:master</q-tooltip>
        </q-icon>
    </template>
</template>

<script setup>
import { mdiAlertDecagramOutline } from "@mdi/js";
const { isNewVersionAvailable, latestVersion, version } = useVersionChecker();
</script>
