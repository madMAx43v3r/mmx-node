<template>
    <q-card flat bordered>
        <q-card-section :class="infoCardSectionDataClass">
            <div v-if="!props.loading">
                <div v-if="value != null">{{ value }}</div>
                <div v-else>&nbsp;</div>
            </div>
            <q-skeleton v-else square width="50%" />
        </q-card-section>
        <q-card-section :class="infoCardSectionTitleClass">
            <div v-if="props.tile.label != null">{{ props.tile.label }}</div>
            <div else>&nbsp;</div>
        </q-card-section>
    </q-card>
</template>

<script setup>
const props = defineProps({
    tile: {
        type: Object,
        default: null,
    },
    rows: {
        type: [Object],
        default: null,
    },
    loading: Boolean,
});

const value = computed(() => {
    var value;
    if (props.tile.field instanceof Function) {
        value = props.tile.field(props.rows);
    } else {
        value = props.rows[props.tile.field];
    }

    const format = props.tile.format;
    if (format) {
        return format(value);
    }
    return value;
});

const infoCardSectionClass = "row justify-center";
const infoCardSectionTitleClass = `${infoCardSectionClass} q-pb-sm q-pt-none text-subtitle1`;
const infoCardSectionDataClass = `${infoCardSectionClass} q-pt-sm q-pb-none text-h6`;
</script>
