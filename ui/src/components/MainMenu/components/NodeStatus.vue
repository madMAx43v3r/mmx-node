<template>
    <div class="q-px-xs">
        <q-icon v-if="statusOptions" :name="statusOptions.icon" size="sm" :class="statusOptions.class">
            <q-tooltip>
                {{ statusOptions.label }}
            </q-tooltip>
        </q-icon>
    </div>
</template>

<script setup>
import { mdiCloudCheckVariant, mdiConnection, mdiEmoticonDead, mdiShieldKey, mdiSync, mdiApiOff } from "@mdi/js";
const { t } = useI18n();

import { useNodeStatus, NodeStatuses } from "@/composables/Node/useNodeStatus";

const options = {
    [NodeStatuses.DisconnectedFromNode]: {
        label: t("node_status.disconnected"),
        icon: mdiEmoticonDead,
        class: "text-negative animate__animated animate__swing animate__infinite",
    },
    [NodeStatuses.QueryTakingLong]: {
        label: "Node response taking too long",
        icon: mdiApiOff,
        class: "text-warning animate__animated animate__flash animate__slow animate__infinite",
    },
    // [statuses.LoggedOff]: {
    //     label: t("node_status.logged_off"),
    //     icon: mdiShieldKey,
    //     class: "text-negative",
    // },
    [NodeStatuses.Connecting]: {
        label: t("node_status.connecting"),
        icon: mdiConnection,
        class: "text-negative animate__animated animate__flash animate__slow animate__infinite",
    },
    [NodeStatuses.Syncing]: {
        label: t("node_status.syncing"),
        icon: mdiSync,
        class: "text-warning spin",
    },
    [NodeStatuses.Synced]: {
        label: t("node_status.synced"),
        icon: mdiCloudCheckVariant,
        class: "text-positive animate__animated animate__bounceIn",
    },
};

const { status } = useNodeStatus();

// import { refThrottled } from "@vueuse/core";
// const throttledStatus = refThrottled(status, 10000);

const statusOptions = computed(() => {
    return options[status.value];
});
</script>

<style lang="scss" scoped>
.spin {
    animation: spin 4s linear infinite;
}
@keyframes spin {
    0% {
        transform: rotate(0deg);
    }
    100% {
        transform: rotate(-360deg);
    }
}
</style>
