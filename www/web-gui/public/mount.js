
const availableLanguages = {
    "en": "English",
    "id": "Bahasa Indonesia",
    //"de": "Deutsch",
    //"es": "Español",
    "nl": "Nederlands",
    "pt": "Português",
    "ru": "Русский",
    //"uk": "Українська"
};

const defaultLocale = 'en';
const loadedLanguages = ['en']

const i18n = VueI18n.createI18n({
    globalInjection: true,
    legacy: false,
    locale: 'en',
    fallbackLocale: 'en',
    messages: { 
        'en': langEN
    }
})

app.use(i18n);

app.mount('#content');


