<template>
    <q-chip square>
        <slot />
        <UseClipboard v-if="copy && defaultSlotContent" v-slot="{ copy: copyX, copied }">
            <q-btn
                :icon-right="mdiContentCopy"
                flat
                size="sm"
                class="q-ml-sm q-pa-none"
                @click="copyX(defaultSlotContent)"
            >
                <q-tooltip :model-value="copied === true" no-parent-event>Copied!</q-tooltip>
            </q-btn>
        </UseClipboard>
    </q-chip>
</template>

<script setup>
import { mdiContentCopy } from "@mdi/js";
import { UseClipboard } from "@vueuse/components";

const props = defineProps({
    copy: {
        type: Boolean,
        default: false,
    },
});

const slots = useSlots();
const defaultSlotContent = computed(() => (slots.default ? slots.default()[0].children : ""));
</script>

<style lang="scss" scoped>
.q-chip {
    height: unset;
    min-height: 28px;
    overflow-wrap: anywhere;
    padding: 0.02em 0.9em;
    margin: 4px 2px;
}

.q-chip:is(:first-child) {
    margin-left: 0px;
}

.q-chip:is(:last-child) {
    margin-right: 0px;
}

.body--dark .q-chip {
    background-color: $blue-grey-10;
}

.body--light .q-chip {
    background-color: $blue-grey-2;
    color: black;
}

::v-deep(.q-chip__content) {
    white-space: unset;
}
</style>
