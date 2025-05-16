<template>
    <q-select
        v-model="word"
        v-bind="$attrs"
        :options="options"
        hide-dropdown-icon
        fill-input
        use-input
        hide-selected
        outlined
        input-debounce="0"
        :rules="[rules.required, inWordList]"
        hide-bottom-space
        no-error-icon
        input-class="text-bold"
        @filter="filterFn"
        @input-value="setModel"
    />
</template>

<script setup>
import rules from "@/helpers/rules";

const word = defineModel({
    type: String,
    required: false,
    default: "",
});

const props = defineProps({
    wordList: {
        type: Array,
        required: true,
    },
});

const options = ref(props.wordList);

const filterFn = (val, update, abort) => {
    update(
        () => {
            const needle = val.toLowerCase();
            options.value = props.wordList.filter((v) => v.toLowerCase().indexOf(needle) == 0);
        },
        // "ref" is the Vue reference to the QSelect
        (ref) => {
            if (val !== "" && ref.options.length > 0 && ref.getOptionIndex() === -1) {
                ref.moveOptionSelection(1, true); // focus the first selectable option and do not update the input-value
                ref.toggleOption(ref.options[ref.optionIndex], true); // toggle the focused option
            }
        }
    );
};

const setModel = (val) => {
    word.value = val;
};

const inWordList = (word) => props.wordList?.includes(word) || "Invalid word";
</script>

<style scoped>
::v-deep(.q-field__bottom) {
    display: none;
}
</style>
