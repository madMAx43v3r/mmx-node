<template>
    <component :is="route.meta.layoutComponent || 'div'">
        <slot />
    </component>
</template>

<script setup>
const route = useRoute();

useAppUpdateChecker();
</script>
