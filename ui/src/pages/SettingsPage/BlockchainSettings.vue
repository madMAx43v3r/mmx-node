<template>
    <q-card flat>
        <q-linear-progress v-if="loading" query class="absolute-top" />
        <q-input
            ref="revertHeightRef"
            v-model="revertHeight"
            :label="$t('node_settings.revert_db_to_height')"
            :rules="[rules.number]"
        />
        <q-btn :disable="btnDisabled" outline color="negative" @click="handleRevertSync(revertHeight)">
            {{ $t("node_settings.revert") }}
        </q-btn>
    </q-card>
</template>

<script setup>
import rules from "@/helpers/rules";

const revertHeightRef = ref();
const revertHeight = ref();

const btnDisabled = computed(() => {
    return !revertHeightRef.value?.validate() || !revertHeight.value;
});

const { t } = useI18n();
const $q = useQuasar();
import { useRevertSync } from "@/queries/api";
const revertSync = useRevertSync();
const handleRevertSync = (height) => {
    height = parseInt(height);
    revertSync.mutate({ height });
};
</script>
