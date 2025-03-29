<template>
    <q-select v-bind="$attrs" emit-value map-options>
        <template v-slot:selected-item="scope">
            <q-item v-bind="scope.itemProps" class="q-pb-none" style="min-height: unset">
                <q-item-section avatar class="content-end q-pb-none">
                    <q-item-label :class="{ 'text-bold': scope.opt.value !== null }">
                        {{ scope.opt.label }}
                    </q-item-label>
                </q-item-section>
                <q-item-section v-if="scope.opt.description">
                    <q-item-label :class="scope.opt.descriptionClasses">
                        {{ scope.opt.description }}
                    </q-item-label>
                </q-item-section>
            </q-item>
        </template>
        <template v-slot:option="scope">
            <q-item v-bind="scope.itemProps">
                <q-item-section avatar class="content-end">
                    <q-item-label>
                        {{ scope.opt.label }}
                    </q-item-label>
                </q-item-section>
                <q-item-section v-if="scope.opt.description">
                    <q-item-label :class="scope.opt.descriptionClasses">
                        {{ scope.opt.description }}
                    </q-item-label>
                </q-item-section>
            </q-item>
        </template>
    </q-select>
</template>

<script setup>
//
</script>
