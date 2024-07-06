
const routes = [
	{ path: '/', redirect: "/explore" },
	{ path: '/explore',
		component: Explore,
		redirect: "/explore/blocks",
		children: [
			{ path: 'blocks', component: ExploreBlocks, meta: { page: 'blocks' } },
			{ path: 'transactions', component: ExploreTransactions, meta: { page: 'transactions' } },
			{ path: 'farmers', component: ExploreFarmers, meta: { page: 'farmers' } },
			{ path: 'block/hash/:hash', component: ExploreBlock, meta: { page: 'block' } },
			{ path: 'block/height/:height', component: ExploreBlock, meta: { page: 'block' } },
			{ path: 'farmer/:id', component: ExploreFarmer, meta: { page: 'farmer' } },
			{ path: 'address/:address', component: ExploreAddress, meta: { page: 'address' } },
			{ path: 'transaction/:id', component: ExploreTransaction, meta: { page: 'transaction' } },
		]
	}
]

const router = new VueRouter({
	routes
})

var vuetify = new Vuetify({
	lang: {
		t: (key, ...params) => i18n.t(key, params),
	},
});

(async () => {

    var locale = localStorage.getItem('language');

    if (locale) {
        await loadLanguageAsync(locale);
    } else {
        setI18nLanguage('en');
    }

    new Vue({
        data: {
            nodeInfo: null
        },
        el: '#app',
        vuetify: vuetify,
        router: router,
        i18n: i18n,
    
        beforeMount() {
            this.$vuetify.theme.dark = localStorage.getItem('theme_dark') === 'true';
        }
    });
})();





