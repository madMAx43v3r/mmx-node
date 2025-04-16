<template>
    <q-markup-table flat>
        <tbody>
            <template v-for="(item, key) in crows" :key="key">
                <tr v-if="item.visible">
                    <td class="key-cell m-bg-grey">{{ item.label }}</td>
                    <td>
                        <slot :name="`value-${item.name}`" :item="item">
                            <template v-if="loading || item?.loading">
                                <q-skeleton key="text" />
                            </template>
                            <template v-else-if="item.to">
                                <RouterLink :to="item.to" :class="`text-primary ${item.classes}`">
                                    {{ item.value }}
                                </RouterLink>
                            </template>
                            <template v-else>
                                <div :class="item.classes">
                                    {{ item.value }}
                                </div>
                            </template>
                            <template v-if="item.copyToClipboard">
                                <UseClipboard v-slot="{ copy, copied }">
                                    <q-btn
                                        :icon-right="mdiContentCopy"
                                        flat
                                        size="sm"
                                        class="q-ml-sm q-pa-none"
                                        @click="copy(item.value)"
                                    >
                                        <q-tooltip :model-value="copied === true" no-parent-event>Copied!</q-tooltip>
                                    </q-btn>
                                </UseClipboard>
                            </template>
                        </slot>
                    </td>
                </tr>
            </template>
        </tbody>
    </q-markup-table>
</template>

<script setup>
import { mdiContentCopy } from "@mdi/js";
import { UseClipboard } from "@vueuse/components";

const props = defineProps({
    rows: {
        type: Object,
        required: true,
    },
    data: {
        type: Object,
        required: false,
        default: null,
    },
    loading: {
        type: Boolean,
        required: false,
        default: false,
    },
});

const { rows, data, loading } = toRefs(props);

const crows = computed(() =>
    props.rows.map((row) => {
        const result = {};

        if (row.visible) {
            result.visible = getPropValue(row.visible, props.data);
        } else {
            result.visible = true;
        }
        if (result.visible) {
            result.value = getValue(row, props.data);

            const properties = Object.keys(row).filter((prop) => ["visible", "field"].indexOf(prop) == -1);
            for (const indx in properties) {
                const propName = properties[indx];
                result[propName] = getPropValue(row[propName], props.data);
            }
        }
        return result;
    })
);

const getValue = (item, data) => {
    var result;
    if (data) {
        if (item.field instanceof Function) {
            try {
                result = item.field(data);
            } catch (e) {
                console.debug(e);
            }
        } else {
            try {
                result = data[item.field];
            } catch (e) {
                console.debug(e);
            }
        }

        if (item.format) {
            result = item.format(result);
        }
    }
    return result;
};

const getPropValue = (prop, data) => {
    var result;
    if (prop instanceof Function) {
        if (data) {
            try {
                result = prop(data);
            } catch (e) {
                console.debug(e);
            }
        }
    } else {
        result = prop;
    }
    return result;
};
</script>
