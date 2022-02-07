
var app = Vue.createApp({
})

const Wallet = {
	template: '<wallet-summary></wallet-summary>'
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
		<account-history :index="index" :limit="200"></account-history>
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
		<account-send-form :index="index"></account-send-form>
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

const Exchange = {
	data() {
		return {
			wallet: null,
			bid_symbol: null,
			ask_symbol: null
		}
	},
	methods: {
		update_wallet(value) {
			this.wallet = value;
		},
		update_bid_symbol(value) {
			this.bid_symbol = value;
		},
		update_ask_symbol(value) {
			this.ask_symbol = value;
		}
	},
	template: `
		<exchange-menu @update-wallet="update_wallet" @update-bid-symbol="update_bid_symbol" @update-ask-symbol="update_ask_symbol"
			:server_="$route.params.server" :bid_="$route.params.bid" :ask_="$route.params.ask" :page="$route.meta.page">
		</exchange-menu>
		<router-view :key="$route.path" :wallet="wallet" :bid_symbol="bid_symbol" :ask_symbol="ask_symbol"></router-view>
		`
}
const ExchangeMarket = {
	props: {
		wallet: null,
		bid_symbol: null,
		ask_symbol: null
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
					:wallet="wallet" :server="$route.params.server"
					:bid_symbol="ask_symbol" :ask_symbol="bid_symbol"
					:bid_currency="$route.params.ask" :ask_currency="$route.params.bid">
				</exchange-trade-form>
			</div>
			<div class="column">
				<exchange-trade-form @trade-executed="update"
					:wallet="wallet" :server="$route.params.server"
					:bid_symbol="bid_symbol" :ask_symbol="ask_symbol"
					:bid_currency="$route.params.bid" :ask_currency="$route.params.ask">
				</exchange-trade-form>
			</div>
		</div>
		<exchange-orders ref="orders" :server="$route.params.server" :bid="$route.params.bid" :ask="$route.params.ask" :limit="100"></exchange-orders>
		`
}

const ExchangeTrades = {
	props: {
		bid_symbol: null,
		ask_symbol: null
	},
	template: `
		<exchange-trades :server="$route.params.server" :bid="$route.params.bid" :ask="$route.params.ask" :limit="200"></exchange-trades>
		`
}
const ExchangeHistory = {
	props: {
		bid_symbol: null,
		ask_symbol: null
	},
	template: `
		<exchange-history :bid="$route.params.bid" :ask="$route.params.ask" :limit="200"></exchange-history>
		`
}
const ExchangeOffers = {
	props: {
		wallet: null,
		bid_symbol: null,
		ask_symbol: null
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
					:wallet="wallet" :server="$route.params.server"
					:bid_symbol="bid_symbol" :ask_symbol="ask_symbol" :flip="false"
					:bid_currency="$route.params.bid" :ask_currency="$route.params.ask">
				</exchange-offer-form>
			</div>
			<div class="column">
				<exchange-offer-form ref="ask_form" @offer-created="update_created"
					:wallet="wallet" :server="$route.params.server"
					:bid_symbol="ask_symbol" :ask_symbol="bid_symbol" :flip="true"
					:bid_currency="$route.params.ask" :ask_currency="$route.params.bid">
				</exchange-offer-form>
			</div>
		</div>
		<exchange-orders ref="orders" :server="$route.params.server" :bid="$route.params.bid" :ask="$route.params.ask" :limit="5"></exchange-orders>
		<account-offers ref="offers" @offer-cancel="update_cancel" :index="wallet" :bid="$route.params.bid" :ask="$route.params.ask"></account-offers>
		`
}

const Settings = { template: '<h1>Settings</h1>TODO' }

const routes = [
	{ path: '/', redirect: "/wallet" },
	{ path: '/wallet', component: Wallet, meta: { is_wallet: true } },
	{ path: '/wallet/account/:index',
		component: Account,
		meta: { is_wallet: true },
		props: route => ({ index: parseInt(route.params.index) }),
		children: [
			{ path: '', component: AccountHome, meta: { page: 'balance' } },
			{ path: 'nfts', component: AccountNFTs, meta: { page: 'nfts' } },
			{ path: 'contracts', component: AccountContracts, meta: { page: 'contracts' } },
			{ path: 'addresses', component: AccountAddresses, meta: { page: 'addresses' } },
			{ path: 'send', component: AccountSend, meta: { page: 'send' } },
			{ path: 'offer', component: AccountOffer, meta: { page: 'offer' } },
		]
	},
	{ path: '/exchange',
		component: Exchange,
		meta: { is_exchange: true },
		children: [
			{ path: 'market/:server/:bid/:ask', component: ExchangeMarket, meta: { page: 'market' } },
			{ path: 'trades/:server/:bid/:ask', component: ExchangeTrades, meta: { page: 'trades' } },
			{ path: 'history/:server/:bid/:ask', component: ExchangeHistory, meta: { page: 'history' } },
			{ path: 'offers/:server/:bid/:ask', component: ExchangeOffers, meta: { page: 'offers' } },
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
	template: `
		<div class="ui large top menu">
			<div class="ui container">
				<router-link class="item" :class="{active: $route.meta.is_wallet}" to="/wallet/">Wallet</router-link>
				<router-link class="item" :class="{active: $route.meta.is_exchange}" to="/exchange/">Exchange</router-link>
				<div class="right menu">
					<router-link class="item" to="/settings/">Settings</router-link>
				</div>
			</div>
		</div>
		`
})

