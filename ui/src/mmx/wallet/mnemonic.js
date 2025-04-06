import { mnemonicToEntropy, entropyToMnemonic } from "@scure/bip39";
import { wordlist as wordlistEnglish } from "@scure/bip39/wordlists/english";

import { randomBytes } from "@noble/hashes/utils";

export const mnemonicToSeed = (mnemonic, wordlist) => {
    if (!wordlist) {
        wordlist = wordlistEnglish;
    }
    return mnemonicToEntropy(mnemonic, wordlist).toReversed();
};

export const seedToWords = (seed, wordlist) => {
    if (!wordlist) {
        wordlist = wordlistEnglish;
    }
    return entropyToMnemonic(seed, wordlist);
};

// 24 words
export const randomSeed = () => randomBytes(256 / 8);
export const randomWords = (wordlist) => {
    if (!wordlist) {
        wordlist = wordlistEnglish;
    }
    return seedToWords(randomSeed(), wordlist);
};

// 12 words
export const randomSeed12 = () => randomBytes(128 / 8);
export const randomWords12 = (wordlist) => {
    if (!wordlist) {
        wordlist = wordlistEnglish;
    }
    return seedToWords(randomSeed12(), wordlist);
};
