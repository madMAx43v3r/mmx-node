<template>
    <q-page padding>
        <h6>BEX Playground</h6>
        <div v-if="isBexLoaded" class="q-gutter-y-sm">
            <q-card flat>
                <q-card-section>
                    <q-btn outline label="Get Height" @click="handleGetHeight()" />
                    <span v-if="height">{{ height }}</span>
                </q-card-section>
            </q-card>

            <q-card flat>
                <q-card-section>
                    <q-btn outline label="Get Accounts" @click="handleGetAccounts()" />
                    <span v-if="accounts">{{ accounts }}</span>
                </q-card-section>
            </q-card>
        </div>
        <div v-else>Extension is not loaded</div>
    </q-page>
</template>

<script setup>
const $q = useQuasar();

const isBexLoaded = computed(() => window.mmx && window.mmx.isFurryVault);

const vault = computed(() => isBexLoaded.value && window.mmx);

const request = async (payload) => {
    try {
        return await vault.value.request(payload);
    } catch (e) {
        $q.notify({ type: "negative", message: e.message });
    }
};

const height = ref(null);
const handleGetHeight = async () => {
    height.value = await request({ method: "mmx_blockNumber" });
};

const accounts = ref(null);
const handleGetAccounts = async () => {
    accounts.value = await request({ method: "mmx_requestAccounts" });
};
</script>
