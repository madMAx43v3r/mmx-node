
const app = Vue.createApp({
})

const Wallet = {
	template: '<wallet-summary/>'
}

const Account = {
	props: {
		index: Number
	},
	template: `
		<account-menu :index="index"/>
		<router-view :index="index"></router-view>
	`
}
const AccountHome = {
	props: {
		index: Number
	},
	template: `
		<balance-table :index="index"/>
		<account-history :index="index"/>
	`
}
const AccountAddresses = {
	props: {
		index: Number
	},
	template: `
		<account-addresses :index="index"/>
	`
}

const Exchange = { template: '<h1>Exchange</h1>TODO' }
const Settings = { template: '<h1>Settings</h1>TODO' }

const routes = [
	{ path: '/', redirect: "/wallet" },
	{ path: '/wallet', component: Wallet },
	{ path: '/wallet/account/:index',
		component: Account,
		props: route => ({ index: parseInt(route.params.index) }),
		children: [
			{ path: '', component: AccountHome },
			{ path: 'addresses', component: AccountAddresses },
		]
	},
	{ path: '/exchange', component: Exchange },
	{ path: '/settings', component: Settings },
]

const router = VueRouter.createRouter({
	history: VueRouter.createWebHashHistory(),
	routes,
})

app.use(router)

app.component('wallet-summary', {
	data() {
		return {
			data: []
		}
	},
	methods: {
		update() {
			fetch('/api/wallet/get_all_accounts')
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update()
	},
	template: `
		<div class="ui raised segment" v-for="item in data" :key="item[0]">
			<account-summary :index="item[0]" :account="item[1]"/>
		</div>
		`
})

app.component('account-menu', {
	props: {
		index: Number
	},
	data() {
		return {
			account: []
		}
	},
	methods: {
		update() {
			fetch('/api/wallet/get_account?index=' + this.index)
				.then(response => response.json())
				.then(data => this.account = data);
		}
	},
	created() {
		this.update()
	},
	template: `
		<account-header :index="index" :account="account"/>
		<div class="ui four item large menu">
			<router-link class="item" :to="'/wallet/account/' + index">Account</router-link>
			<router-link class="item" :to="'/wallet/account/' + index">NFTs</router-link>
			<router-link class="item" :to="'/wallet/account/' + index">Contracts</router-link>
			<router-link class="item" :to="'/wallet/account/' + index + '/addresses'">Addresses</router-link>
		</div>
		`
})

app.component('account-header', {
	props: {
		index: Number,
		account: Object
	},
	data() {
		return {
			address: null
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/address?index=' + this.index)
				.then(response => response.json())
				.then(data => this.address = data[0]);
		}
	},
	created() {
		this.update()
	},
	template: `
		<router-link class="ui large labels" :to="'/wallet/account/' + index">
			<div class="ui horizontal label">#{{index}}</div>
			<div class="ui horizontal label">{{account.name}}</div>
			<div class="ui horizontal label">{{address}}</div>
		</router-link>
		`
})

app.component('account-summary', {
	props: {
		index: Number,
		account: Object
	},
	template: `
		<div>
			<account-header :index="index" :account="account"/>
			<balance-table :index="index"/>
		</div>
		`
})

app.component('balance-table', {
	props: {
		index: Number
	},
	data() {
		return {
			data: {
				balances: []
			}
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/balance?index=' + this.index)
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update()
	},
	template: `
		<table class="ui table">
			<thead>
			<tr>
				<th class="two wide">Balance</th>
				<th class="two wide">Token</th>
				<th>Contract</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data.balances" :key="item.contract">
				<td>{{item.value}}</td>
				<td>{{item.symbol}}</td>
				<td>{{item.is_native ? '' : item.contract}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('account-history', {
	props: {
		index: Number
	},
	data() {
		return {
			data: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/history?limit=20&index=' + this.index)
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update()
	},
	template: `
		<table class="ui table striped">
			<thead>
			<tr>
				<th class="collapsing">Height</th>
				<th class="collapsing">Type</th>
				<th class="collapsing">Amount</th>
				<th>Token</th>
				<th>Address</th>
				<th>Time</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data">
				<td>{{item.height}}</td>
				<td>{{item.type}}</td>
				<td>{{item.value}}</td>
				<td>{{item.symbol}}</td>
				<td>{{item.address}}</td>
				<td>{{new Date(item.time * 1000).toLocaleString()}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('account-addresses', {
	props: {
		index: Number
	},
	data() {
		return {
			data: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/address?limit=100&index=' + this.index)
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update()
	},
	template: `
		<table class="ui table striped">
			<thead>
			<tr>
				<th class="collapsing right">Index</th>
				<th>Address</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="(item, index) in data" :key="index">
				<td>{{index}}</td>
				<td>{{item}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.mount('#content');
