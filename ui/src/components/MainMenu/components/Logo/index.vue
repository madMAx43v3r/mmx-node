<template>
    <div class="logo">
        <SvgLogo height="18" :color="logoColor" />
        <div class="mono node-height">
            <div v-if="isOffline" class="text-negative">offline</div>
            <template v-else>
                {{ height }}
            </template>
        </div>
    </div>
</template>

<script setup>
const nodeStore = useNodeStore();
const height = computed(() => nodeStore.height && nodeStore.height.toString().replace(/\B(?=(\d{3})+(?!\d))/g, " "));

// eslint-disable-next-line no-undef
const isOffline = __BUILD_TARGET__ === "OFFLINE";

const appStore = useAppStore();
const logoColor = computed(() => (appStore.isDarkTheme ? "#fff" : "#000"));
</script>

<style lang="scss" scoped>
.logo {
    font-size: 21px;
    font-weight: 700;
    letter-spacing: 4px;
    position: relative;
    display: inline-block;
}

.node-height {
    font-size: 10px;
    letter-spacing: normal;
    position: absolute;
    bottom: -6px;
    right: 0px;
    color: var(--q-primary);
}
</style>
