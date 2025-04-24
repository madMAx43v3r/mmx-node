<template>
    <tr v-for="n in props.rowCount" :key="n">
        <td v-for="col in props.props.cols" :key="col" :class="getClasses(col.classes)">
            <q-skeleton key="text" />
        </td>
    </tr>
</template>

<script setup>
const props = defineProps({
    props: {
        type: Object,
        required: true,
    },
    rowCount: {
        type: Number,
        default: 10,
    },
    loading: Boolean,
});

const getClasses = (classes) => {
    return classes instanceof Function ? "" : classes;
};
</script>
