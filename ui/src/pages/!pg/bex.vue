<template>
    <div>
        <h6>BEX Playground</h6>
        <div v-if="isBexLoaded">
            <div>
                <q-btn outline label="Get Height" @click="handleGetHeight()" />
                Height: {{ height }}
            </div>
        </div>
        <div v-else>Extension is not loaded</div>
    </div>
</template>

<script setup>
const isBexLoaded = computed(() => window.mmx && window.mmx.isFurryVault);

const vault = computed(() => window.mmx);

const height = ref(null);
const handleGetHeight = async () => {
    height.value = await vault.value.request({ method: "mmx_blockNumber" });
};
</script>
