
function validate_address(address) {
	return address && address.length == 62 && address.startsWith("mmx1");
}

var app = Vue.createApp({
	beforeMount() {
		(async () => {

			var locale = 'en';

			if(this.isWinGUI) {
				locale = window.mmx.locale;
				setInterval( () => { 
					locale = window.mmx.locale;	
					if (i18n.locale != locale) {
						loadLanguageAsync(locale);
					}
				}, 1000);
			} else {
				locale = localStorage.getItem('language')
			}

			if (locale) {
				await loadLanguageAsync(locale);
			} else {
				setI18nLanguage('en');
			}

		})()
	}
});

app.config.globalProperties.isWinGUI = navigator.userAgent.indexOf("mmx.gui.win") > 0;

const Wallet = {
	template: '<wallet-summary></wallet-summary>'
}
const WalletCreate = {
	template: '<create-wallet></create-wallet>'
}
const Account = {
	props: {
		index: Number
	},
	template: `
		<account-header :index="index"></account-header>
		<account-menu :index="index"></account-menu>
		<router-view :index="index"></router-view>
	`
}
const AccountHome = {
	props: {
		index: Number
	},
	template: `
		<account-balance :index="index"></account-balance>
		<account-history :index="index" :limit="50"></account-history>
	`
}
const AccountNFTs = {
	props: {
		index: Number
	},
	template: `
		<nft-table :index="index"></nft-table>
	`
}
const AccountContracts = {
	props: {
		index: Number
	},
	template: `
		<create-contract-menu :index="index"></create-contract-menu>
		<account-contracts :index="index"></account-contracts>
	`
}
const AccountAddresses = {
	props: {
		index: Number
	},
	template: `
		<account-addresses :index="index" :limit="1000"></account-addresses>
	`
}
const AccountSend = {
	props: {
		index: Number
	},
	template: `
		<account-send-form :index="index" :target_="$route.params.target"></account-send-form>
	`
}
const AccountSendFrom = {
	props: {
		index: Number
	},
	template: `
		<account-send-form :index="index" :source_="$route.params.source"></account-send-form>
	`
}
const AccountSplit = {
	props: {
		index: Number
	},
	template: `
		<account-split-form :index="index"></account-split-form>
	`
}
const AccountOffer = {
	props: {
		index: Number
	},
	template: `
		<account-offer-form :index="index"></account-offer-form>
	`
}
const AccountHistory = {
	props: {
		index: Number
	},
	template: `
		<account-history :index="index" :limit="200"></account-history>
	`
}
const AccountLog = {
	props: {
		index: Number
	},
	template: `
		<account-tx-history :index="index" :limit="200"></account-tx-history>
	`
}
const AccountDetails = {
	props: {
		index: Number
	},
	template: `
		<account-details :index="index"></account-details>
	`
}
const AccountOptions = {
	props: {
		index: Number
	},
	template: `
		<account-actions :index="index"></account-actions>
		<create-account :index="index"></create-account>
	`
}
const AccountCreateLocked = {
	props: {
		index: Number
	},
	template: `
		<create-locked-contract :index="index"></create-locked-contract>
	`
}
const AccountCreateVirtualPlot = {
	props: {
		index: Number
	},
	template: `
		<create-virtual-plot-contract :index="index"></create-virtual-plot-contract>
	`
}

const Market = {
	props: {
		wallet: Number,
		bid: null,
		ask: null,
	},
	template: `
		<market-menu :wallet_="wallet" :bid_="bid" :ask_="ask" :page="$route.meta.page"></market-menu>
		<router-view :key="$route.path" :wallet="wallet" :bid="bid" :ask="ask"></router-view>
		`
}
const MarketOffers = {
	props: {
		wallet: Number,
		bid: null,
		ask: null,
	},
	template: `
		<market-offers ref="orders" :wallet="wallet" :bid="bid" :ask="ask" :limit="200"></market-offers>
		`
}

const Exchange = {
	props: {
		wallet: Number,
		server: String,
		bid: String,
		ask: String,
	},
	data() {
		return {
			bid_symbol: null,
			ask_symbol: null
		}
	},
	methods: {
		update_bid_symbol(value) {
			this.bid_symbol = value;
		},
		update_ask_symbol(value) {
			this.ask_symbol = value;
		}
	},
	template: `
		<exchange-menu @update-bid-symbol="update_bid_symbol" @update-ask-symbol="update_ask_symbol"
			:wallet_="wallet" :server_="server" :bid_="bid" :ask_="ask" :page="$route.meta.page">
		</exchange-menu>
		<router-view :key="$route.path" :wallet="wallet" :server="server" :bid="bid" :ask="ask" :bid_symbol="bid_symbol" :ask_symbol="ask_symbol"></router-view>
		`
}
const ExchangeMarket = {
	props: {
		wallet: Number,
		server: String,
		bid: String,
		ask: String,
		bid_symbol: String,
		ask_symbol: String
	},
	methods: {
		update() {
			this.$refs.orders.update();
		}
	},
	template: `
		<div class="ui two column grid">
			<div class="column">
				<exchange-trade-form @trade-executed="update"
					:wallet="wallet" :server="server"
					:bid_symbol="ask_symbol" :ask_symbol="bid_symbol"
					:bid_currency="ask" :ask_currency="bid" :action="'Buy ' + bid_symbol">
				</exchange-trade-form>
			</div>
			<div class="column">
				<exchange-trade-form @trade-executed="update"
					:wallet="wallet" :server="server"
					:bid_symbol="bid_symbol" :ask_symbol="ask_symbol"
					:bid_currency="bid" :ask_currency="ask" :action="'Sell ' + bid_symbol">
				</exchange-trade-form>
			</div>
		</div>
		<exchange-orders ref="orders" :server="server" :bid="bid" :ask="ask" :flip="false" :limit="100"></exchange-orders>
		`
}

const ExchangeTrades = {
	props: {
		server: String,
		bid: String,
		ask: String,
		bid_symbol: String,
		ask_symbol: String
	},
	template: `
		<exchange-trades :server="server" :bid="bid" :ask="ask" :limit="200"></exchange-trades>
		`
}
const ExchangeHistory = {
	props: {
		server: String,
		bid: String,
		ask: String,
		bid_symbol: String,
		ask_symbol: String
	},
	template: `
		<exchange-history :bid="bid" :ask="ask" :limit="200"></exchange-history>
		`
}
const ExchangeOffers = {
	props: {
		wallet: Number,
		server: String,
		bid: String,
		ask: String,
		bid_symbol: String,
		ask_symbol: String
	},
	methods: {
		update_created() {
			this.$refs.orders.update();
			this.$refs.offers.update();
		},
		update_cancel() {
			this.$refs.orders.update();
			this.$refs.bid_form.update();
			this.$refs.ask_form.update();
		}
	},
	template: `
		<div class="ui two column grid">
			<div class="column">
				<exchange-offer-form ref="bid_form" @offer-created="update_created"
					:wallet="wallet" :server="server"
					:bid_symbol="bid_symbol" :ask_symbol="ask_symbol" :flip="false"
					:bid_currency="bid" :ask_currency="ask">
				</exchange-offer-form>
			</div>
			<div class="column">
				<exchange-offer-form ref="ask_form" @offer-created="update_created"
					:wallet="wallet" :server="server"
					:bid_symbol="ask_symbol" :ask_symbol="bid_symbol" :flip="true"
					:bid_currency="ask" :ask_currency="bid">
				</exchange-offer-form>
			</div>
		</div>
		<exchange-orders ref="orders" :server="server" :bid="bid" :ask="ask" :flip="true" :limit="5"></exchange-orders>
		<account-offers ref="offers" @offer-cancel="update_cancel" :index="wallet" :bid="bid" :ask="ask"></account-offers>
		`
}

const Explore = {
	template: `
		<explore-menu></explore-menu>
		<router-view></router-view>
	`
}
const ExploreBlocks = {
	template: `
		<explore-blocks :limit="100"></explore-blocks>
	`
}
const ExploreTransactions = {
	template: `
		<explore-transactions :limit="100"></explore-transactions>
	`
}
const ExploreBlock = {
	template: `
		<block-view :hash="$route.params.hash" :height="parseInt($route.params.height)"></block-view>
	`
}
const ExploreAddress = {
	template: `
		<address-view :address="$route.params.address"></address-view>
	`
}
const ExploreTransaction = {
	template: `
		<transaction-view :id="$route.params.id"></transaction-view>
	`
}

const Node = {
	template: `
		<node-info></node-info>
		<node-menu></node-menu>
		<router-view></router-view>
	`
}
const NodeBlocks = {
	template: `
		<explore-blocks :limit="20"></explore-blocks>
	`
}
const NodeLog = {
	template: `
		<node-log></node-log>
	`
}
const NodePeers = {
	template: `
		<node-peers></node-peers>
	`
}
const NodeNetspace = {
	template: `
		<netspace-graph :limit="1000" :step="90"></netspace-graph>
	`
}
const NodeVDFSpeed = {
	template: `
		<vdf-speed-graph :limit="1000" :step="90"></vdf-speed-graph>
	`
}
const NodeBlockReward = {
	template: `
		<block-reward-graph :limit="1000" :step="90"></block-reward-graph>
	`
}

const Settings = {
	template: `
		<node-settings></node-settings>
	`
}

const Login = {
	data() {
		return {
			passwd: null,
			error: null
		}
	},
	methods: {
		submit() {
			this.error = null;
			fetch('/server/login?user=mmx-admin&passwd_plain=' + this.passwd)
				.then(response => {
					if(response.ok) {
						this.$router.push("/");
					} else {
						this.error = "Login failed!";
					}
				});
		}
	},
	template: `
		<div class="ui raised segment">
			<form class="ui form">
				<div class="field">
					<label>{{ $t('login.password_label') }}</label>
					<input type="password" v-model="passwd">
				</div>
				<div @click="submit" class="ui submit button">{{ $t('login.login') }}</div>
			</form>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			<b>{{error}}</b>
		</div>
		`
}

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
		component: Node,
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
	{ path: '/settings', component: Settings },
]

const router = VueRouter.createRouter({
	history: VueRouter.createWebHashHistory(),
	routes,
})

app.use(router)

app.component('main-menu', {
	methods: {
		update() {
			this.loading = true;
			fetch('/server/session')
				.then(response => response.json())
				.then(data => {
					if(!data.user) {
						this.$router.push("/login");
					}
				});
		},
		logout() {
			fetch('/server/logout')
				.then(response => {
					if(response.ok) {
						this.$router.push("/login");
					}
				});
		}
	},
	created() {
		if(!this.isWinGUI) 
		{
			this.update();
			this.timer = setInterval(() => { this.update(); }, 5000);
		}
	},
	mounted() {
		if(this.isWinGUI) 
		{
			$("body").css("background-color", "#f2f2f2")
			$('#content').css("padding", "10px 10px 100px 10px");
			$('#content').css("float", "left");
		}
	},	
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<div class="ui large top menu">
			<div class="ui container">
				<router-link class="item" :class="{active: $route.meta.is_node}" to="/node/">{{ $t('main_menu.node') }}</router-link>
				<router-link class="item" :class="{active: $route.meta.is_wallet}" to="/wallet/">{{ $t('main_menu.wallet') }}</router-link>
				<router-link class="item" :class="{active: $route.meta.is_explorer}" to="/explore/">{{ $t('main_menu.explore') }}</router-link>
				<router-link class="item" :class="{active: $route.meta.is_market}" to="/market/">{{ $t('main_menu.market') }}</router-link>
				<!--<router-link class="item" :class="{active: $route.meta.is_exchange}" to="/exchange/">{{ $t('main_menu.exchange') }}</router-link>-->
				<div class="right menu">
					<router-link class="item" to="/settings/">{{ $t('main_menu.settings') }}</router-link>
					<template v-if="!$route.meta.is_login && !isWinGUI">
						<a class="item" @click="logout">{{ $t('main_menu.logout') }}</a>
					</template>
				</div>
			</div>
		</div>
		`
})

