<template>
    <q-dialog ref="dialogRef" persistent @show="onDialogShow" @hide="onDialogHide">
        <q-card class="q-dialog-plugin" style="width: 800px; max-width: 100vw">
            <q-toolbar>
                <q-toolbar-title> </q-toolbar-title>

                <q-btn v-close-popup flat round dense :icon="mdiClose" />
            </q-toolbar>
            <q-card-section class="q-pa-none">
                <div class="q-gutter-y-sm">
                    <SeedInputForm ref="formRef" v-model:wallet="wallet" @login="handleLogin" />
                </div>
            </q-card-section>
        </q-card>
    </q-dialog>
</template>

<script setup>
import { mdiClose } from "@mdi/js";

import { useDialogPluginComponent } from "quasar";
defineEmits([...useDialogPluginComponent.emits]);
const { dialogRef, onDialogHide, onDialogOK, onDialogCancel } = useDialogPluginComponent();

const formRef = ref(null);
const onDialogShow = () => {
    if (formRef.value) {
        formRef.value.onDialogShow();
    }
};

const wallet = ref(null);
const handleLogin = () => {
    onDialogOK({ wallet });
};

/* Usage:

const $q = useQuasar();
const handleLogin = () => {
    $q.dialog({
        component: defineAsyncComponent(() => import("@/components/Dialogs/SeedInputDialog")),
        componentProps: {},
    }).onOk(({ wallet }) => {
        console.log(wallet);
    });
};

*/
</script>
