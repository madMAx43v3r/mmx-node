<template>
    <RouterView v-slot="{ Component }">
        <transition name="custom-classes" enter-active-class="animate__animated animate__fadeIn">
            <component :is="Component" v-if="show" />
        </transition>
    </RouterView>
</template>

<script setup>
const props = defineProps({
    show: {
        type: Boolean,
        default: true,
        required: false,
    },
});
</script>
