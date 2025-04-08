<template>
    <div>
        <h6>BEX Playground</h6>
        <div v-if="isBexLoaded">
            <div>
                <q-btn outline label="Get Height" @click="handleGetHeight()" />
                <span v-if="height">Height: {{ height }}</span>
            </div>
        </div>
        <div v-else>Extension is not loaded</div>
    </div>
</template>

<script setup>
const $q = useQuasar();

const isBexLoaded = computed(() => window.mmx && window.mmx.isFurryVault);

const vault = computed(() => window.mmx);

const height = ref(null);
const handleGetHeight = async () => {
    try {
        height.value = await vault.value.request({ method: "mmx_blockNumber" });
    } catch (e) {
        $q.notify({ type: "negative", message: e.message });
    }
};
</script>
