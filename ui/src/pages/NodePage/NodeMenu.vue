<template>
    <q-tabs align="left" indicator-color="primary" outside-arrows class="m-bg-grey">
        <template v-for="(item, key) in tabs" :key="key">
            <q-route-tab
                v-if="item.visible ?? true"
                :to="item.to"
                :exact="item.exact ?? false"
                :label="item.label"
                :icon="item.icon"
            />
        </template>
    </q-tabs>
</template>

<script setup>
const { isLocalNode } = useConfigData();

const { t } = useI18n();
const tabs = computed(() => [
    { to: "/node/log", label: t("node_menu.log") },
    { to: "/node/peers", label: t("node_menu.peers"), visible: isLocalNode.value },
    { to: "/node/blocks", label: t("node_menu.blocks") },
    { to: "/node/netspace", label: t("node_menu.netspace") },
    { to: "/node/vdf_speed", label: t("node_menu.vdf_speed") },
    { to: "/node/reward", label: t("node_menu.block_reward") },
]);
</script>
