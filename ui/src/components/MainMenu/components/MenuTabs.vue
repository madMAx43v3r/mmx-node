<template>
    <q-tabs indicator-color="primary" outside-arrows class="q-mx-xs">
        <template v-for="(item, key) in tabs" :key="key">
            <template v-if="item.separator">
                <q-separator vertical inset class="q-mx-md" />
            </template>
            <template v-else-if="item.space">
                <q-space />
            </template>
            <template v-else-if="item.component">
                <template v-if="item.tabTagWrap ?? true">
                    <div class="q-tab self-stretch flex flex-center text-center">
                        <component :is="item.component" />
                    </div>
                </template>
                <template v-else>
                    <component :is="item.component" />
                </template>
            </template>
            <template v-else>
                <q-route-tab
                    v-if="item.visible ?? true"
                    :to="item.to"
                    :label="item.label"
                    :icon="item.icon"
                    :disable="item.disable"
                    @click="item.click"
                />
            </template>
        </template>
    </q-tabs>
</template>

<script setup>
const props = defineProps({
    tabs: {
        type: Array,
        required: true,
    },
});
</script>
