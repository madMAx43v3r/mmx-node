
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
        'en': langEN
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

function customFallback(messages) {
    function nestedLoop(obj1, obj2) {
        const res = {};
        function recurse(obj1, obj2, res) {
            for (const key in obj1) {
                let value1 = obj1[key];
                let value2 = obj2[key];
                if(value1 != undefined) {
                    if (value1 && typeof value1 === 'object') {
                        if(value2) {
                            res[key] = value2;
                            recurse(value1, value2, res[key]);
                        } else {
                            res[key] = value1;
                        }                        
                    } else {
                        //console.log(key, value1, value2);
                        if(value2) {
                            res[key] = value2;
                        } else {
                            res[key] = value1;
                        }
                    }
                }
            }
        }
        recurse(obj1,obj2,res);
        return res;
    }

    messages = nestedLoop(langEN, messages)
    return messages;
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

