<template>
    <q-dialog ref="dialogRef" persistent @hide="onDialogHide" @show="onDialogShow">
        <q-card class="q-dialog-plugin">
            <q-card-section>
                <div class="q-gutter-y-sm">
                    <!-- // TODO i18n -->
                    <div class="text-h6">Unlock wallet</div>
                    <q-input
                        ref="inputRef"
                        v-model="passphrase"
                        :hint="$t('wallet_common.enter_passphrase')"
                        outlined
                        :type="!showPassphrase ? 'password' : 'text'"
                        autocomplete="new-password"
                        @keyup.enter="onOKClick"
                    >
                        <template v-slot:append>
                            <q-icon
                                :name="!showPassphrase ? mdiEyeOff : mdiEye"
                                class="cursor-pointer"
                                @click="showPassphrase = !showPassphrase"
                            />
                        </template>
                    </q-input>
                </div>
            </q-card-section>
            <q-card-actions align="right">
                <q-btn label="Cancel" flat @click="onDialogCancel" />
                <q-btn label="Unlock" flat :icon="mdiLockOpenVariant" @click="onOKClick" />
            </q-card-actions>
        </q-card>
    </q-dialog>
</template>

<script setup>
import { mdiLockOpenVariant, mdiEye, mdiEyeOff } from "@mdi/js";
const passphrase = ref("");
const showPassphrase = ref(false);

import { useDialogPluginComponent } from "quasar";
defineEmits([...useDialogPluginComponent.emits]);
const { dialogRef, onDialogHide, onDialogOK, onDialogCancel } = useDialogPluginComponent();

const onOKClick = () => {
    onDialogOK({ passphrase: passphrase.value });
};

const inputRef = ref(null);
const onDialogShow = () => {
    if (inputRef.value) {
        inputRef.value.focus();
    }
};
</script>
