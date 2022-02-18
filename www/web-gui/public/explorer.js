
app.component('recent-blocks-summary', {
	props: {
		limit: Number
	},
	data() {
		return {
			data: null,
			timer: null,
			loading: false
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/headers?limit=' + this.limit)
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					for(const block of data) {
						if(block.tx_base) {
							block.reward = 0;
							for(const out of block.tx_base.outputs) {
								block.reward += out.amount;
							}
							block.reward /= 1000000;
						}
					}
					this.data = data;
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 10000);
	},
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<template v-if="!data && loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<table class="ui table striped" v-if="data">
			<thead>
			<tr>
				<th>Height</th>
				<th>TX</th>
				<th>K</th>
				<th>Score</th>
				<th>Reward</th>
				<th>T-Diff</th>
				<th>S-Diff</th>
				<th>Hash</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data" :key="item.key">
				<td>{{item.height}}</td>
				<td>{{item.tx_count}}</td>
				<td>{{item.proof ? item.proof.ksize : ""}}</td>
				<td>{{item.proof ? item.proof.score : ""}}</td>
				<td><template v-if="item.reward">{{(item.reward).toPrecision(6)}}</template></td>
				<td>{{item.time_diff}}</td>
				<td>{{item.space_diff}}</td>
				<td>{{item.hash}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('transaction-view', {
	props: {
		id: String
	},
	data() {
		return {
			data: null,
			timer: null,
			loading: false
		}
	},
	methods: {
		update() {
			if(this.id) {
				this.loading = true;
				fetch('/wapi/transaction?id=' + this.id)
					.then(response => response.json())
					.then(data => {
						this.loading = false;
						this.data = data;
					});
			}
		}
	},
	watch: {
		id() {
			this.data = null;
			this.update();
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 10000);
	},
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<div class="ui large labels">
			<div class="ui horizontal label">Transaction</div>
			<div class="ui horizontal label">{{id}}</div>
		</div>
		<template v-if="!data && !loading">
			<div class="ui large negative message">
				No such transaction!
			</div>
		</template>
		<template v-if="!data && loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<template v-if="data">
			<table class="ui definition table striped">
				<tbody>
				<tr>
					<td class="two wide">Height</td>
					<td colspan="2">
						<template v-if="data.height">{{data.height}}</template>
						<template v-if="!data.height"><i>pending</i></template>
					</td>
				</tr>
				<tr v-if="data.confirm">
					<td class="two wide">Confirmed</td>
					<td colspan="2">{{data.confirm}}</td>
				</tr>
				<tr v-if="data.time">
					<td class="two wide">Time</td>
					<td colspan="2">{{new Date(data.time * 1000).toLocaleString()}}</td>
				</tr>
				<tr v-if="data.deployed">
					<td class="two wide">Address</td>
					<td colspan="2">{{data.address}}</td>
				</tr>
				<tr>
					<td class="two wide">Cost</td>
					<td class="collapsing"><b>{{data.cost.value}}</b></td>
					<td>MMX</td>
				</tr>
				<tr>
					<td class="two wide">Fee</td>
					<td class="collapsing"><b>{{data.fee.value}}</b></td>
					<td>MMX</td>
				</tr>
				</tbody>
			</table>
			<table class="ui definition table striped" v-if="data.fee.value >= 0">
				<thead>
				<tr>
					<th></th>
					<th>Amount</th>
					<th>Token</th>
					<th>Previous</th>
				</tr>
				</thead>
				<tbody>
				<tr v-for="(item, index) in data.inputs" :key="index">
					<td class="two wide">Input[{{index}}]</td>
					<td class="collapsing"><b>{{item.utxo.value}}</b></td>
					<td>{{item.utxo.symbol}}</td>
					<td><router-link :to="'/explore/transaction/' + item.prev.txid">{{item.prev.key}}</router-link></td>
				</tr>
				</tbody>
			</table>
			<table class="ui definition table striped">
				<thead>
				<tr>
					<th></th>
					<th>Amount</th>
					<th>Token</th>
					<th>Address</th>
					<th>Spent</th>
				</tr>
				</thead>
				<tbody>
				<tr v-for="(item, index) in data.outputs" :key="index">
					<td class="two wide">Output[{{index}}]</td>
					<td class="collapsing"><b>{{item.output.value}}</b></td>
					<td>{{item.output.symbol}}</td>
					<td>{{item.output.address}}</td>
					<td><template v-if="item.spent"><router-link :to="'/explore/transaction/' + item.spent.txid">Next</router-link></template></td>
				</tr>
				</tbody>
			</table>
			<contract-table :data="data.deployed" v-if="data.deployed"></contract-table>
		</template>
		`
})

app.component('contract-table', {
	props: {
		data: Object
	},
	template: `
		<table class="ui definition table striped">
			<tbody>
			<template v-for="(value, key) in data" :key="key">
				<tr v-if="key != '__type'">
					<td class="collapsing">{{key}}</td>
					<td>{{value}}</td>
				</tr>
			</template>
			</tbody>
		</table>
		`
})
