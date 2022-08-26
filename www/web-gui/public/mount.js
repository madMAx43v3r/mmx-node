
const routes = [
	{ path: '/', redirect: "/node" },
	{ path: '/login', component: Login, meta: { is_login: true } },
	{ path: '/wallet', component: Wallet, meta: { is_wallet: true } },
	{ path: '/wallet/create', component: WalletCreate, meta: { is_wallet: true } },
	{ path: '/wallet/account/:index',
		component: Account,
		meta: { is_wallet: true },
		props: route => ({ index: parseInt(route.params.index) }),
		children: [
			{ path: '', component: AccountHome, meta: { page: 'balance' } },
			{ path: 'nfts', component: AccountNFTs, meta: { page: 'nfts' } },
			{ path: 'contracts', component: AccountContracts, meta: { page: 'contracts' } },
			{ path: 'addresses', component: AccountAddresses, meta: { page: 'addresses' } },
			{ path: 'send/:target?', component: AccountSend, meta: { page: 'send' } },
			{ path: 'send_from/:source?', component: AccountSendFrom, meta: { page: 'send' } },
			{ path: 'split', component: AccountSplit, meta: { page: 'split' } },
			{ path: 'offer', component: AccountOffer, meta: { page: 'offer' } },
			{ path: 'history', component: AccountHistory, meta: { page: 'history' } },
			{ path: 'log', component: AccountLog, meta: { page: 'log' } },
			{ path: 'details', component: AccountDetails, meta: { page: 'details' } },
			{ path: 'options', component: AccountOptions, meta: { page: 'options' } },
			{ path: 'create/locked', component: AccountCreateLocked },
			{ path: 'create/virtualplot', component: AccountCreateVirtualPlot },
		]
	},
	{ path: '/market',
		component: Market,
		meta: { is_market: true },
		props: route => ({
			wallet: parseInt(route.params.wallet),
			bid: route.params.bid == 'null' ? null : route.params.bid,
			ask: route.params.ask == 'null' ? null : route.params.ask,
		}),
		children: [
			{ path: 'offers/:wallet/:bid/:ask', component: MarketOffers, meta: { page: 'offers' } },
		]
	},
	{ path: '/exchange',
		component: Exchange,
		meta: { is_exchange: true },
		props: route => ({
			wallet: parseInt(route.params.wallet),
			server: route.params.server,
			bid: route.params.bid,
			ask: route.params.ask
		}),
		children: [
			{ path: 'market/:wallet/:server/:bid/:ask', component: ExchangeMarket, meta: { page: 'market' } },
			{ path: 'trades/:wallet/:server/:bid/:ask', component: ExchangeTrades, meta: { page: 'trades' } },
			{ path: 'history/:wallet/:server/:bid/:ask', component: ExchangeHistory, meta: { page: 'history' } },
			{ path: 'offers/:wallet/:server/:bid/:ask', component: ExchangeOffers, meta: { page: 'offers' } },
		]
	},
	{ path: '/explore',
		component: Explore,
		redirect: "/explore/blocks",
		meta: { is_explorer: true },
		children: [
			{ path: 'blocks', component: ExploreBlocks, meta: { page: 'blocks' } },
			{ path: 'transactions', component: ExploreTransactions, meta: { page: 'transactions' } },
			{ path: 'block/hash/:hash', component: ExploreBlock, meta: { page: 'block' } },
			{ path: 'block/height/:height', component: ExploreBlock, meta: { page: 'block' } },
			{ path: 'address/:address', component: ExploreAddress, meta: { page: 'address' } },
			{ path: 'transaction/:id', component: ExploreTransaction, meta: { page: 'transaction' } },
		]
	},
	{ path: '/node',
		component: NodeView,
		redirect: "/node/log",
		meta: { is_node: true },
		children: [
			{ path: 'log', component: NodeLog, meta: { page: 'log' } },
			{ path: 'peers', component: NodePeers, meta: { page: 'peers' } },
			{ path: 'blocks', component: NodeBlocks, meta: { page: 'blocks' } },
			{ path: 'netspace', component: NodeNetspace, meta: { page: 'netspace' } },
			{ path: 'vdf_speed', component: NodeVDFSpeed, meta: { page: 'vdf_speed' } },
			{ path: 'reward', component: NodeBlockReward, meta: { page: 'reward' } },
		]
	},
	{ path: '/settings', component: Settings, meta: { is_settings: true } },
]

const router = new VueRouter({
	routes
})

var vuetify = new Vuetify();

Vue.prototype.$isWinGUI = typeof window.mmx !== 'undefined';

var app = new Vue({
	el: '#app',
	vuetify: vuetify,
	router: router,
    i18n: i18n,

	beforeMount() {
		(async () => {
					
			var locale;

			if(this.$isWinGUI) {
				
				locale = window.mmx.locale;
				this.$vuetify.theme.dark = window.mmx.theme_dark;

				setInterval( () => { 
					var locale = window.mmx.locale;	
					if (i18n.locale != locale) {
						loadLanguageAsync(locale);
					}

					this.$vuetify.theme.dark = window.mmx.theme_dark;
				}, 1000);
			} else {
				locale = localStorage.getItem('language');
				this.$vuetify.theme.dark = localStorage.getItem('theme_dark') === 'true';
			}

			if (locale) {
				await loadLanguageAsync(locale);
			} else {
				setI18nLanguage('en');
			}

		})()
	},

	beforeDestroy() {

	}

});



