import { mnemonicToEntropy, entropyToMnemonic } from "@scure/bip39";
import { wordlist as wordlistEnglish } from "@scure/bip39/wordlists/english";

import * as secp256k1 from "@noble/secp256k1";
const { randomBytes } = secp256k1.etc;

const mnemonicToSeed = (mnemonic, wordlist) => {
    if (!wordlist) {
        wordlist = wordlistEnglish;
    }
    return mnemonicToEntropy(mnemonic, wordlist).toReversed();
};

const seedToWords = (seed, wordlist) => {
    if (!wordlist) {
        wordlist = wordlistEnglish;
    }
    return entropyToMnemonic(seed, wordlist);
};

const randomSeed = () => randomBytes(32);

const randomWords = (wordlist) => {
    if (!wordlist) {
        wordlist = wordlistEnglish;
    }
    return seedToWords(randomSeed(), wordlist);
};

export { mnemonicToSeed, seedToWords, randomWords };
