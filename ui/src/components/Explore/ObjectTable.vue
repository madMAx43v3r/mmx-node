<template>
    <q-markup-table flat>
        <tbody>
            <template v-for="(value, key) in data" :key="key">
                <tr v-if="key != '__type'">
                    <td class="key-cell m-bg-grey">{{ key }}</td>
                    <td class="textwarp mono">
                        <template v-if="value instanceof Object || key == 'source'">
                            <q-btn
                                v-if="isExpandable(value, key)"
                                :icon="!expanded[key] ? mdiArrowExpand : mdiArrowCollapse"
                                fab-mini
                                class="float-right"
                                @click="handleToggleExpand(key)"
                            />
                            <highlightjs
                                :code="stringify(value)"
                                :class="{ hljsCode: true, collapsed: !expanded[key] }"
                            />
                        </template>
                        <template v-else>
                            <template v-if="isAddress(value)">
                                <RouterLink :to="`/explore/address/${value}`" class="text-primary mono">
                                    {{ value }}
                                </RouterLink>
                            </template>
                            <template v-else-if="isHex(value)">
                                <div class="hex">{{ value }}</div>
                            </template>
                            <template v-else>
                                {{ value }}
                            </template>
                        </template>
                    </td>
                </tr>
            </template>
        </tbody>
    </q-markup-table>
</template>

<script setup>
import { mdiArrowExpand, mdiArrowCollapse } from "@mdi/js";

const props = defineProps({
    data: {
        type: Object,
        default: null,
    },
});
const expanded = ref({});
const stringify = (value) => (value instanceof Object ? JSON.stringify(value, null, 4) : value);
const isAddress = (value) => typeof value === "string" && value.startsWith("mmx1") && value.length == 62;
const isExpandable = (value, key) => (value instanceof Object && Object.keys(value).length > 0) || key == "source";

const handleToggleExpand = (key) => {
    if (!expanded.value[key]) {
        expanded.value[key] = false;
    }
    expanded.value[key] = !expanded.value[key];
};

const isHex = (value) => value && value.length > 68 && value.length % 2 == 0 && /^[0-9a-fA-F]+$/.test(value);
</script>

<style scoped>
.textwarp {
    text-wrap: wrap;
    overflow-wrap: anywhere;
}

.collapsed {
    white-space: normal;
    /* font-size: 0.8em; */
}

::v-deep(.hljs) {
    background: transparent !important;
    padding: 0px !important;
}

::v-deep(pre.hljsCode) {
    margin: 0px !important;
}

.hex {
    font-family: monospace;
    font-size: 0.8em;
}
</style>
