import globals from "globals";
import pluginJs from "@eslint/js";
import pluginVue from "eslint-plugin-vue";
import pluginQuery from "@tanstack/eslint-plugin-query";
import eslintPluginPrettierRecommended from "eslint-plugin-prettier/recommended";

// ------------------------------------------------------------------
// https://eslint.org/docs/latest/use/configure/migration-guide#using-eslintrc-configs-in-flat-config
// ------------------------------------------------------------------
import { FlatCompat } from "@eslint/eslintrc";
import path from "path";
import { fileURLToPath } from "url";

// mimic CommonJS variables -- not needed if using CommonJS
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const compat = new FlatCompat({
    baseDirectory: __dirname,
});
// ------------------------------------------------------------------
import { includeIgnoreFile } from "@eslint/compat";
const gitignorePath = path.resolve(__dirname, ".gitignore");

export default [
    includeIgnoreFile(gitignorePath),

    { languageOptions: { globals: globals.browser } },

    pluginJs.configs.recommended,
    ...pluginVue.configs["flat/recommended"],
    ...pluginQuery.configs["flat/recommended"],
    eslintPluginPrettierRecommended,

    ...compat.extends("./.eslintrc-auto-import.json"),
    {
        rules: {
            "prettier/prettier": "warn",
            "no-unused-vars": "off",

            "vue/multi-word-component-names": "off",
            "vue/v-slot-style": "off",
        },
    },
];
