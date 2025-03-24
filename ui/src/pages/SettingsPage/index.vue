<template>
    <q-page padding>
        <div class="q-gutter-y-sm">
            <q-list bordered class="rounded-borders">
                <template v-for="panel in panels" :key="panel">
                    <q-expansion-item
                        v-model="panelState[panel.name]"
                        :label="panel.label"
                        :icon="panel.icon"
                        header-class="text-h6"
                        expand-separator
                        group="expansionGroup"
                    >
                        <q-card>
                            <q-card-section>
                                <component :is="panel.component" />
                            </q-card-section>
                        </q-card>
                    </q-expansion-item>
                </template>
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
    mdiExpansionCard,
    mdiDatabaseOutline,
    mdiHandCoinOutline,
    mdiApplicationCogOutline,
} from "@mdi/js";

import NodeSettings from "./NodeSettings";
import RewardSettings from "./RewardSettings";
import HarvesterSettings from "./HarvesterSettings";
import CudaSettings from "./CudaSettings";
import BlockchainSettings from "./BlockchainSettings";
import WalletSettings from "./WalletSettings";
import UISettings from "./UISettings";
import BuildVersionInfo from "./BuildVersionInfo.vue";

const appStore = useAppStore();

const { data, isLocalNode, isFarmer } = useConfigData();

const { t } = useI18n();
const panels = [
    {
        name: "ui",
        label: t("node_settings.ui"),
        icon: mdiApplicationCogOutline,
        component: UISettings,
        visible: !appStore.isWinGUI,
    },
    {
        name: "node",
        label: "Node",
        icon: mdiTools,
        component: NodeSettings,
        visible: isLocalNode.value,
    },
    {
        name: "reward",
        label: "Reward",
        icon: mdiCashMultiple,
        component: RewardSettings,
        visible: isLocalNode.value || isFarmer.value,
    },
    {
        name: "harvester",
        label: t("harvester_settings.harvester"),
        icon: mdiTractorVariant,
        component: HarvesterSettings,
        visible: isFarmer.value,
    },
    {
        name: "cuda",
        label: "CUDA",
        icon: mdiExpansionCard,
        component: CudaSettings,
        visible: isFarmer.value || isLocalNode.value,
    },
    {
        name: "blockchain",
        label: t("node_settings.blockchain"),
        icon: mdiDatabaseOutline,
        component: BlockchainSettings,
        visible: isLocalNode.value,
    },
    {
        name: "wallet",
        label: t("wallet_settings.token_whitelist"),
        icon: mdiHandCoinOutline,
        component: WalletSettings,
        visible: true,
    },
];

const firstVisiblePanel = computed(() => panels.find((panel) => panel.visible));

const router = useRouter();
const route = useRoute();

const initPanelState = computed(() => {
    const state = route.query.state || firstVisiblePanel.value?.name;

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
</script>
