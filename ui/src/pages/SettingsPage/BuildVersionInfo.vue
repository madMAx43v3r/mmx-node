<template>
    <q-card flat>
        <q-card-section>
            <div class="text-h6">
                <q-avatar :icon="mdiArchiveCogOutline" />
                {{ $t("build_version.build") }}
            </div>
            <div class="q-gutter-y-sm">
                <ListTable :rows="rows" :data="data" :loading="loading" />

                <q-banner v-if="isNewVersionAvailable" rounded inline-actions class="bg-warning">
                    This build is {{ version }}, compared to {{ latestVersion }} at madMAx43v3r:master
                </q-banner>
            </div>
        </q-card-section>
    </q-card>
</template>

<script setup>
import { mdiArchiveCogOutline } from "@mdi/js";

const { data, loading } = useConfigData();

const { t } = useI18n();
const rows = computed(() => [
    { label: t("build_version.version"), field: "version" },
    { label: t("build_version.commit"), field: "commit" },
]);

const { isNewVersionAvailable, latestVersion, latestRelease, version } = useVersionChecker();
</script>
