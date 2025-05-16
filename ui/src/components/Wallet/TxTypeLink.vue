<template>
    <RouterLink :to="link">
        <slot />
    </RouterLink>
</template>

<script setup>
const props = defineProps({
    id: {
        type: String,
        required: true,
    },
    type: {
        type: String,
        required: true,
    },
});

const link = computed(() => {
    switch (props.type) {
        case "REWARD":
        case "VDF_REWARD":
        case "PROJECT_REWARD":
            return "/explore/block/hash/" + props.id;
        default:
            return "/explore/transaction/" + props.id;
    }
});
</script>
