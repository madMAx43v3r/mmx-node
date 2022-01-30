
const app = Vue.createApp({
})

app.component('wallet-summary', {
	data() {
		return {
			data: []
		}
	},
	methods: {
		update() {
			fetch('/api/wallet/get_accounts')
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update()
	},
	template: `<account-summary v-for="item in data" :index="item[0]" :account="item[1]"></account-summary>`
})

app.component('account-summary', {
	props: {
		index: Number,
		account: Object
	},
	template: `
		<div class="ui raised segment">
			<div class="item">
				<div class="ui horizontal label">#{{index}}</div>
				{{account.name}}
			</div>
			<balance-table :index="index"></balance-table>
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
				nfts: [],
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
				<th>Balance</th>
				<th>Token</th>
				<th>Contract</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data.balances">
				<td>{{item.value}}</td>
				<td>{{item.symbol}}</td>
				<td>{{item.contract ? item.contract : ""}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.mount('#content');
