
const availableLanguages = [
    { code: "en", language: "English" },
    { code: "id", language: "Bahasa Indonesia" },
    { code: "de", language: "Deutsch" },
    { code: "es", language: "Español" },
    { code: "nl", language: "Nederlands" },
    { code: "pt", language: "Português" },
    { code: "ru", language: "Русский" },
    { code: "uk", language: "Українська" },
    { code: "zh", language: "简体中文" }
];

const defaultLocale = 'en';
const loadedLanguages = ['en']

const i18n = new VueI18n({
    globalInjection: true,
    legacy: false,
    locale: 'en',
    fallbackLocale: 'en',
    messages: { 
        'en': mergeDeep({}, commonLocale, enLocale)
    }
})

function setI18nLanguage(locale) {

    i18n.locale = locale

    /**
     * NOTE:
     * If you need to specify the language setting for headers, such as the `fetch` API, set it here.
     * The following is an example for axios.
     *
     * axios.defaults.headers.common['Accept-Language'] = locale
     */
    document.querySelector('html').setAttribute('lang', locale)
}


function isObject(item) {
    return (item && typeof item === 'object' && !Array.isArray(item));
}
  
function mergeDeep(target, ...sources) {

    if (!sources.length) return target;
    const source = sources.shift();

    if (isObject(target) && isObject(source)) {
        for (const key in source) {
            if (isObject(source[key])) {
                if (!target[key]) Object.assign(target, {
                    [key]: {}
                });
                mergeDeep(target[key], source[key]);
            } else {
                Object.assign(target, {
                    [key]: source[key]
                });
            }
        }
    }

    return mergeDeep(target, ...sources);
}


function customFallback(msg) {
    let result = mergeDeep({}, commonLocale, enLocale, msg)
    return result;
}   

function loadLanguageAsync(locale) {
    //console.log('loadLanguageAsync', locale);

	//if (i18n.locale === locale) {
	//	return Promise.resolve(setI18nLanguage(locale))
	//}

	if (loadedLanguages.includes(locale)) {
		return Promise.resolve(setI18nLanguage(locale))
	}

    if (availableLanguages.filter( lang => lang.code == locale ).length > 0 ) {
        return fetch(`./locales/${locale}.json`).then(
            response => response.json()     
        ).then(
            messages => {            
                messages = customFallback(messages);
                i18n.setLocaleMessage(locale, messages);
                //console.log('setLocaleMessage', locale, messages);
                loadedLanguages.push(locale)
                return setI18nLanguage(locale)
            },
        )
    } else {
        return Promise.resolve(setI18nLanguage(locale))
    }
}

