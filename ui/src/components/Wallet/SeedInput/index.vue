<template>
    <div>
        <div class="row q-row-gutter-xs q-col-gutter-xs">
            <template v-for="(word, index) in words" :key="index">
                <SeedWord
                    v-model="words[index]"
                    :word-list="wordList"
                    :label="(index + 1).toString()"
                    :readonly="readonly"
                    class="col-md-2 col-sm-3 col-xs-4"
                    @update:model-value="(value) => handleWordUpdate(value, index)"
                />
            </template>
        </div>
    </div>
</template>

<script setup>
const seedString = defineModel({
    type: String,
    required: false,
    default: "",
});
const props = defineProps({
    readonly: {
        type: Boolean,
        required: false,
        default: false,
    },
});

const wordCount = 24;

const words = ref(new Array(wordCount).fill(""));

const handleWordUpdate = (value, index) => {
    //console.log(value, index);
    value = value.trim();
    const _wordCount = value.split(" ").length;
    if (_wordCount == 1) {
        seedString.value = words.value.join(" ");
    } else if (_wordCount == wordCount) {
        seedString.value = value;
        updateWords();
    }
};

const updateWords = () => {
    if (!seedString.value) {
        words.value = new Array(wordCount).fill("");
    } else {
        words.value = seedString.value.split(" ").slice(0, wordCount);
    }
};

watchEffect(() => updateWords());
import { wordlist as wordList } from "@scure/bip39/wordlists/english";
</script>
