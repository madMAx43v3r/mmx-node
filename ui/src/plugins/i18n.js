import { createI18n } from "vue-i18n";
import commonMessages from "@/locales/common.json";
import enMessages from "@/locales/en-US.json";
import enQLocale from "/node_modules/quasar/lang/en-US.js";

export const availableLanguages = [
    { value: "en-US", label: "English" },
    { value: "id", label: "Bahasa Indonesia" },
    { value: "de", label: "Deutsch" },
    { value: "es", label: "Español" },
    { value: "nl", label: "Nederlands" },
    { value: "pt", label: "Português" },
    { value: "ru", label: "Русский" },
    { value: "uk", label: "Українська" },
    { value: "zh-CN", label: "简体中文" },
];

export const defaultLocale = availableLanguages[0].value;

// const availableLanguagesList = availableLanguages.map((item) => item.value);
// console.log(availableLanguagesList.join("|"));

// https://quasar.dev/options/quasar-language-packs/#dynamical-non-ssr-
import { Lang } from "quasar";
const langList = import.meta.glob("/node_modules/quasar/lang/(en-US|id|de|es|nl|pt|ru|uk|zh-CN).js");

const setI18nLanguage = (i18n, locale) => {
    if (i18n.mode === "legacy") {
        i18n.global.locale = locale;
    } else {
        i18n.global.locale.value = locale;
    }
    /**
     * NOTE:
     * If you need to specify the language setting for headers, such as the `fetch` API, set it here.
     * The following is an example for axios.
     *
     * axios.defaults.headers.common['Accept-Language'] = locale
     */
    document.querySelector("html").setAttribute("lang", locale);
};

export const loadAndSetI18nLanguage = async (i18n, _locale) => {
    const locale = toValue(validateLocale(_locale));
    try {
        langList[`/node_modules/quasar/lang/${locale}.js`]().then((lang) => {
            Lang.set(lang.default);
        });
    } catch (err) {
        console.error(err);
        Lang.set(enQLocale.default);
    }
    try {
        const messages = await import(`@/locales/${locale}.json`);
        i18n.global.setLocaleMessage(locale, mergeDeep({}, commonMessages, messages));
        setI18nLanguage(i18n, locale);
    } catch (err) {
        i18n.global.setLocaleMessage(defaultLocale, mergeDeep({}, commonMessages, enMessages));
        setI18nLanguage(i18n, defaultLocale);
    }
};

export const validateLocale = (locale) => {
    if (availableLanguages.some((language) => language.value == toValue(locale))) {
        return locale;
    } else {
        return defaultLocale;
    }
};

const isObject = (item) => {
    return item && typeof item === "object" && !Array.isArray(item);
};

const mergeDeep = (target, ...sources) => {
    if (!sources.length) return target;
    const source = sources.shift();

    if (isObject(target) && isObject(source)) {
        for (const key in source) {
            if (isObject(source[key])) {
                if (!target[key])
                    Object.assign(target, {
                        [key]: {},
                    });
                mergeDeep(target[key], source[key]);
            } else {
                Object.assign(target, {
                    [key]: source[key],
                });
            }
        }
    }

    return mergeDeep(target, ...sources);
};

const i18n = createI18n({
    globalInjection: true,
    legacy: false,
    locale: defaultLocale,
    fallbackLocale: defaultLocale,
    messages: {
        "en-US": mergeDeep({}, commonMessages, enMessages),
    },
});

export default i18n;
