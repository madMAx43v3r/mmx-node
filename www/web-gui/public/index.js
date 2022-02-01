
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

const Exchange = { template: '<h1>Exchange</h1>TODO' }
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
		]
	},
	{ path: '/exchange', component: Exchange, meta: { is_exchange: true } },
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

