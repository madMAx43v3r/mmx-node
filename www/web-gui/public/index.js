
const app = Vue.createApp({
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
			<account-summary :index="item[0]" :account="item[1]"></account-summary>
		</div>
		`
})

app.component('account-menu', {
	props: {
		index: Number
	},
	template: `
		<div class="ui five item large pointing menu">
			<router-link class="item" :class="{active: $route.meta.page == 'balance'}" :to="'/wallet/account/' + index">Balance</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'nfts'}" :to="'/wallet/account/' + index + '/nfts'">NFTs</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'contracts'}" :to="'/wallet/account/' + index + '/contracts'">Contracts</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'addresses'}" :to="'/wallet/account/' + index + '/addresses'">Addresses</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'send'}" :to="'/wallet/account/' + index + '/send'">Send</router-link>
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
			info: {
				name: null,
				index: null
			},
			address: null
		}
	},
	methods: {
		update() {
			if(this.account) {
				this.info = this.account
			} else {
				fetch('/api/wallet/get_account?index=' + this.index)
					.then(response => response.json())
					.then(data => this.info = data);
			}
			fetch('/wapi/wallet/address?index=' + this.index)
				.then(response => response.json())
				.then(data => this.address = data[0]);
		}
	},
	created() {
		this.update()
	},
	template: `
		<div class="ui large labels">
			<div class="ui horizontal label">Account</div>
			<div class="ui horizontal label">#{{index}}</div>
			<div class="ui horizontal label">{{info.name}}</div>
			<div class="ui horizontal label">{{info.index}}</div>
			<div class="ui horizontal label">{{address}}</div>
		</div>
		`
})

app.component('account-summary', {
	props: {
		index: Number,
		account: Object
	},
	template: `
		<div>
			<account-header :index="index" :account="account"></account-header>
			<account-menu :index="index"></account-menu>
			<account-balance :index="index"></account-balance>
		</div>
		`
})

app.component('account-balance', {
	props: {
		index: Number
	},
	data() {
		return {
			balances: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/balance?index=' + this.index)
				.then(response => response.json())
				.then(data => this.balances = data.balances);
		}
	},
	created() {
		this.update()
	},
	template: `
		<balance-table :balances="balances"></balance-table>
		`
})

app.component('balance-table', {
	props: {
		balances: Object
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
			<tr v-for="item in balances" :key="item.contract">
				<td>{{item.value}}</td>
				<td>{{item.symbol}}</td>
				<td>{{item.is_native ? '' : item.contract}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('nft-table', {
	props: {
		index: Number
	},
	data() {
		return {
			nfts: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/balance?index=' + this.index)
				.then(response => response.json())
				.then(data => this.nfts = data.nfts);
		}
	},
	created() {
		this.update()
	},
	template: `
		<table class="ui table">
			<thead>
			<tr>
				<th>NFT</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in nfts" :key="item">
				<td>{{item}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('account-history', {
	props: {
		index: Number,
		limit: Number
	},
	data() {
		return {
			data: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/history?limit=' + this.limit + '&index=' + this.index)
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
				<th>Height</th>
				<th>Type</th>
				<th>Amount</th>
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

app.component('contract-summary', {
	props: {
		address: String,
		contract: Object
	},
	data() {
		return {
			balances: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/address?id=' + this.address)
				.then(response => response.json())
				.then(data => this.balances = data.balances);
		}
	},
	created() {
		this.update()
	},
	template: `
		<div class="ui raised segment">
			<div class="ui large labels">
				<div class="ui horizontal label">{{contract.__type}}</div>
				<div class="ui horizontal label">{{address}}</div>
			</div>
			<table class="ui definition table striped">
				<tbody>
				<template v-for="(value, key) in contract" :key="key">
					<tr v-if="key != '__type'">
						<td class="collapsing">{{key}}</td>
						<td>{{value}}</td>
					</tr>
				</template>
				</tbody>
			</table>
			<balance-table :balances="balances" v-if="balances.length"></balance-table>
		</div>
		`
})

app.component('account-contracts', {
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
			fetch('/wapi/wallet/contracts?index=' + this.index)
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update()
	},
	template: `
		<contract-summary v-for="item in data" :key="item[0]" :address="item[0]" :contract="item[1]"></contract-summary>
		`
})

app.component('account-addresses', {
	props: {
		index: Number,
		limit: Number
	},
	data() {
		return {
			data: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/address?limit=' + this.limit + '&index=' + this.index)
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
				<th>Index</th>
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

app.component('account-send-form', {
	props: {
		index: Number
	},
	data() {
		return {
			accounts: [],
			balances: [],
			amount: null,
			confirmed: false,
			options: {
				split_output: 1
			}
		}
	},
	methods: {
		update() {
			fetch('/api/wallet/get_all_accounts')
				.then(response => response.json())
				.then(data => {
					for(const entry of data) {
						const info = entry[1];
						info.account = entry[0];
						fetch('/wapi/wallet/address?limit=1&index=' + entry[0])
							.then(response => response.json())
							.then(data => {
								info.address = data[0];
								this.accounts.push(info);
								this.accounts.sort(function(a, b){return a.account - b.account});
							});
					}
				});
			fetch('/wapi/wallet/balance?index=' + this.index)
				.then(response => response.json())
				.then(data => this.balances = data.balances);
		},
		submit() {
			this.confirmed = false;
			const req = {};
			req.index = this.index;
			req.amount = parseFloat(this.amount);
			req.currency = $('#currency_input').val();
			req.dst_addr = $('#target').val();
			req.options = this.options;
			fetch('/wapi/wallet/send', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						response.json().then(data => {
							alert("Transaction ID: " + data)
						});
					} else {
						response.text().then(data => {
							alert("Send failed with: " + data)
						});
					}
					this.update();
					this.$refs.balance.update();
				});
		}
	},
	created() {
		this.update()
	},
	mounted() {
		$('#address').dropdown({
			onChange: function(value, text) {
				if(value) {
					$('#target').val(value).prop('disabled', true);
				} else {
					$('#target').val("").prop('disabled', false);
				}
			}
		});
		$('#currency').dropdown({
			onChange: function(value, text) {}
		});
		$('.ui.checkbox').checkbox();
	},
	watch: {
		amount(value) {
			// TODO: validate
		},
		confirmed(value) {
			if(value) {
				$('#submit').removeClass('disabled');
			} else {
				$('#submit').addClass('disabled');
			}
		}
	},
	template: `
		<account-balance :index="index" ref="balance"></account-balance>
		<div class="ui raised segment">
		<form class="ui form" id="form">
			<div class="field">
				<label>Destination</label>
				<div class="ui selection dropdown" id="address">
					<i class="dropdown icon"></i>
					<div class="default text">Select</div>
					<div class="menu">
						<div class="item" data-value="">Address Input</div>
						<div v-for="item in accounts" :key="item.account" class="item" :data-value="item.address">
							Account #{{item.account}} ({{item.name}}) [{{item.index}}] ({{item.address}})
						</div>
					</div>
				</div>
			</div>
			<div class="two fields">
				<div class="fourteen wide field">
					<label>Destination Address</label>
					<input type="text" id="target" placeholder="mmx1..."/>
				</div>
				<div class="two wide field">
					<label>Output Split</label>
					<input type="text" v-model.number="options.split_output" style="text-align: right"/>
				</div>
			</div>
			<div class="two fields">
				<div class="four wide field">
					<label>Amount</label>
					<input type="text" v-model="amount" placeholder="1.23" style="text-align: right"/>
				</div>
				<div class="twelve wide field">
					<label>Currency</label>
					<div class="ui selection dropdown" id="currency">
						<input type="hidden" id="currency_input">
						<i class="dropdown icon"></i>
						<div class="default text">Select</div>
						<div class="menu">
							<div v-for="item in balances" :key="item.contract" class="item" :data-value="item.contract">
								{{item.symbol}} <template v-if="!item.is_native"> - [{{item.contract}}]</template>
							</div>
						</div>
					</div>
				</div>
			</div>
			<div class="inline field">
				<div class="ui toggle checkbox">
					<input type="checkbox" class="hidden" v-model="confirmed">
					<label>Confirm</label>
				</div>
			</div>
			<div @click="submit" class="ui submit primary button disabled" id="submit">Send</div>
		</form>
		</div>
		`
})

app.mount('#content')
