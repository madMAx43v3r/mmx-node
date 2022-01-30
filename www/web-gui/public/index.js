
const app = Vue.createApp({
})

app.component('balance-table', {
props: {
	address: String
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
	<table class="ui table">
		<thead>
		<tr>
			<th>Balance</th>
			<th>Token</th>
		</tr>
		</thead>
		<tbody>
		<tr v-for="(item, index) in balances">
			<td>{{item.value}}</td>
			<td>{{item.symbol}}</td>
		</tr>
		</tbody>
	</table>
	`
})

app.mount('#content');
