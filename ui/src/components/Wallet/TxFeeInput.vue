<template>
    <q-input
        :model-value="fee"
        v-bind="$attrs"
        :label="$t('account_history_form.tx_fee')"
        suffix="MMX"
        input-class="amount-input"
        readonly
        class="col-2"
    >
        <q-inner-loading :showing="loading">
            <q-spinner-dots color="grey" size="2em" />
        </q-inner-loading>

        <q-inner-loading :showing="locked">
            <q-icon :name="mdiLockOpenAlert" size="sm" class="text-warning">
                <q-tooltip>Unlock wallet to see estimated fee</q-tooltip>
            </q-icon>
        </q-inner-loading>

        <q-inner-loading :showing="error != null">
            <q-icon :name="mdiAlert" size="sm" class="text-warning">
                <q-tooltip>{{ error }}</q-tooltip>
            </q-icon>
        </q-inner-loading>
    </q-input>
</template>

<script setup>
import { mdiLockOpenAlert, mdiAlert } from "@mdi/js";

const fee = defineModel({
    type: Number,
    required: false,
    default: null,
});

const props = defineProps({
    error: {
        type: String,
        required: false,
        default: null,
    },
    loading: {
        type: Boolean,
        required: false,
        default: false,
    },
    locked: {
        type: Boolean,
        required: false,
        default: false,
    },
});

const loadingE = computed(() => props.loading && props.error === null);
</script>

<style scoped>
.q-inner-loading {
    backdrop-filter: blur(1px);
    background-color: transparent;
}
</style>
