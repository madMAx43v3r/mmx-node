<template>
    <q-card flat>
        <q-card-section class="text-center">
            <q-icon :name="props.icon" size="xl" />
            <div class="text-h5 q-mt-sm">
                {{ title }}
            </div>
        </q-card-section>
    </q-card>
</template>

<script setup>
const props = defineProps({
    icon: {
        type: String,
        default: null,
        required: false,
    },
    title: {
        type: String,
        default: null,
        required: false,
    },
});
</script>
