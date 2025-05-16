<template>
    <q-dialog ref="dialogRef" persistent @hide="onDialogHide">
        <q-card class="q-dialog-plugin" style="width: 800px; max-width: 100vw">
            <q-card-section>
                <div class="q-gutter-y-sm">
                    <SeedInput :model-value="seed" readonly />
                    <q-input
                        v-if="withPassphrase"
                        :model-value="fingerPrint"
                        label="Fingerprint (needed to verify passphrase)"
                        input-class="text-bold"
                        readonly
                    />
                </div>
            </q-card-section>
            <q-card-actions align="right">
                <UseClipboard v-slot="{ copy, copied }">
                    <q-btn label="Copy" flat :icon="mdiContentCopy" @click="copy(seed)">
                        <q-tooltip :model-value="copied === true" no-parent-event>Copied!</q-tooltip>
                    </q-btn>
                </UseClipboard>
                <q-btn label="Ok" flat @click="onDialogOK" />
            </q-card-actions>
        </q-card>
    </q-dialog>
</template>

<script setup>
import { mdiContentCopy } from "@mdi/js";
import { UseClipboard } from "@vueuse/components";

const props = defineProps({
    seed: {
        type: String,
        required: true,
    },
    withPassphrase: {
        type: Boolean,
        required: false,
        default: false,
    },
    fingerPrint: {
        type: String,
        required: false,
        default: null,
    },
});

import { useDialogPluginComponent } from "quasar";
defineEmits([...useDialogPluginComponent.emits]);
const { dialogRef, onDialogHide, onDialogOK, onDialogCancel } = useDialogPluginComponent();
</script>
