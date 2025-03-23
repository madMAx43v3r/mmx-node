<template>
    <q-page padding>
        <div class="q-gutter-y-sm">
            <q-list bordered class="rounded-borders">
                <q-expansion-item
                    v-if="isLocalNode"
                    v-model="panelState['node']"
                    expand-separator
                    label="Node"
                    :icon="mdiTools"
                    :header-class="headerClass"
                    group="expansionGroup"
                >
                    <q-card>
                        <q-card-section>
                            <NodeSettings />
                        </q-card-section>
                    </q-card>
                </q-expansion-item>

                <q-expansion-item
                    v-if="isLocalNode || isFarmer"
                    v-model="panelState['reward']"
                    expand-separator
                    label="Reward"
                    :icon="mdiCashMultiple"
                    :header-class="headerClass"
                    group="expansionGroup"
                >
                    <q-card>
                        <q-card-section>
                            <RewardSettings />
                        </q-card-section>
                    </q-card>
                </q-expansion-item>

                <q-expansion-item
                    v-if="isFarmer"
                    v-model="panelState['harvester']"
                    expand-separator
                    :label="$t('harvester_settings.harvester')"
                    :icon="mdiTractorVariant"
                    :header-class="headerClass"
                    group="expansionGroup"
                >
                    <q-card>
                        <q-card-section>
                            <HarvesterSettings />
                        </q-card-section>
                    </q-card>
                </q-expansion-item>

                <q-expansion-item
                    v-if="isLocalNode"
                    v-model="panelState['blockchain']"
                    expand-separator
                    :label="$t('node_settings.blockchain')"
                    :icon="mdiDatabaseOutline"
                    :header-class="headerClass"
                    group="expansionGroup"
                >
                    <q-card>
                        <q-card-section>
                            <BlockchainSettings />
                        </q-card-section>
                    </q-card>
                </q-expansion-item>

                <q-expansion-item
                    v-model="panelState['wallet']"
                    expand-separator
                    :label="$t('wallet_settings.token_whitelist')"
                    :icon="mdiHandCoinOutline"
                    :header-class="headerClass"
                    group="expansionGroup"
                >
                    <q-card>
                        <q-card-section>
                            <WalletSettings />
                        </q-card-section>
                    </q-card>
                </q-expansion-item>

                <q-expansion-item
                    v-if="!appStore.isWinGUI"
                    v-model="panelState['ui']"
                    expand-separator
                    :label="$t('node_settings.ui')"
                    :icon="mdiApplicationCogOutline"
                    :header-class="headerClass"
                    group="expansionGroup"
                >
                    <q-card>
                        <q-card-section>
                            <UISettings />
                        </q-card-section>
                    </q-card>
                </q-expansion-item>
            </q-list>

            <BuildVersionInfo class="q-mt-md" />
        </div>
    </q-page>
</template>

<script setup>
import {
    mdiTools,
    mdiCashMultiple,
    mdiTractorVariant,
    mdiDatabaseOutline,
    mdiHandCoinOutline,
    mdiApplicationCogOutline,
} from "@mdi/js";

import NodeSettings from "./NodeSettings";
import RewardSettings from "./RewardSettings";
import HarvesterSettings from "./HarvesterSettings";
import BlockchainSettings from "./BlockchainSettings";
import WalletSettings from "./WalletSettings";
import UISettings from "./UISettings";
import BuildVersionInfo from "./BuildVersionInfo.vue";

const headerClass = "text-h6";

const appStore = useAppStore();

const router = useRouter();
const route = useRoute();

const state = route.query.state || "node";
const initPanelState = computed(() => {
    const res = {};
    res[state] = true;
    return res;
});
const panelState = ref(initPanelState.value);

watch(
    panelState,
    (value) => {
        var keys = Object.keys(value);
        if (value) {
            router.push({ path: route.path, query: { state: keys.filter((key) => value[key])[0] }, replace: true });
        } else {
            router.push({ path: route.path, replace: true });
        }
    },
    { deep: true }
);

const { data, isLocalNode, isFarmer } = useConfigData();
</script>
