
app.component('explore-menu', {
	data() {
		return {
			input: null,
			error: null
		}
	},
	methods: {
		submit() {
			const hex = /[0-9A-Fa-f]{8}/g;
			if(this.input) {
				this.error = null;
				if(this.input.startsWith("mmx1")) {
					this.$router.push("/explore/address/" + this.input);
				}
				else if(hex.test(this.input)) {
					if(this.input.length == 64) {
						this.$router.push("/explore/transaction/" + this.input);
					} else {
						this.error = true;
					}
				}
				else if(parseInt(this.input) != NaN) {
					this.$router.push("/explore/block/height/" + this.input);
				}
				else {
					this.error = true;
				}
			}
		}
	},
	template: `
		<div class="ui large pointing menu">
			<router-link class="item" :class="{active: $route.meta.page == 'blocks'}" to="/explore/blocks">Blocks</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'transactions'}" to="/explore/transactions">Transactions</router-link>
			<div class="item" style="flex-grow:1;">
				<div class="ui transparent icon input" :class="{error: !!error}">
					<input type="text" v-model="input" v-on:keyup.enter="submit" placeholder="address | transaction id | block height">
					<i class="search link icon" @click="submit"></i>
				</div>
			</div>
		</div>
	`
})

app.component('explore-blocks', {
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
			<tr v-for="item in data" :key="item.hash">
				<td><router-link :to="'/explore/block/height/' + item.height">{{item.height}}</router-link></td>
				<td>{{item.tx_count}}</td>
				<td>{{item.proof ? item.proof.ksize : ""}}</td>
				<td>{{item.proof ? item.proof.score : ""}}</td>
				<td><template v-if="item.reward">{{(item.reward).toPrecision(6)}}</template></td>
				<td>{{item.time_diff}}</td>
				<td>{{item.space_diff}}</td>
				<td><router-link :to="'/explore/block/hash/' + item.hash">{{item.hash}}</router-link></td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('explore-transactions', {
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
			fetch('/wapi/transactions?limit=' + this.limit)
				.then(response => response.json())
				.then(data => {
					this.loading = false;
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
				<th>Type</th>
				<th>Fee</th>
				<th>N(in)</th>
				<th>N(out)</th>
				<th>N(op)</th>
				<th>Transaction ID</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data" :key="item.id">
				<td><router-link :to="'/explore/block/height/' + item.height">{{item.height}}</router-link></td>
				<td>{{item.note ? item.note : ""}}</td>
				<td><b>{{item.fee.value}}</b></td>
				<td>{{item.inputs.length}}</td>
				<td>{{item.outputs.length}}</td>
				<td>{{item.operations.length}}</td>
				<td><router-link :to="'/explore/transaction/' + item.id">{{item.id}}</router-link></td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('block-view', {
	props: {
		hash: String,
		height: Number
	},
	data() {
		return {
			data: null,
			loading: false
		}
	},
	methods: {
		update() {
			let url = null;
			if(this.hash) {
				url = '/wapi/block?hash=' + this.hash;
			}
			if(this.height >= 0) {
				url = '/wapi/block?height=' + this.height;
			}
			if(url) {
				this.loading = true;
				fetch(url)
					.then(response => {
						if(response.ok) {
							response.json()
								.then(data => {
									this.loading = false;
									this.data = data;
								});
						} else {
							this.loading = false;
							this.data = null;
						}
					});
			}
		}
	},
	watch: {
		hash() {
			this.update();
		},
		height() {
			this.update();
		}
	},
	created() {
		this.update();
	},
	template: `
		<template v-if="data">
			<div class="ui large pointing menu">
				<router-link class="item" :to="'/explore/block/hash/' + data.prev">Previous</router-link>
				<router-link class="right item" :to="'/explore/block/height/' + (data.height + 1)">Next</router-link>
			</div>
			<div class="ui large labels">
				<div class="ui horizontal label">Block</div>
				<div class="ui horizontal label">{{data.height}}</div>
				<div class="ui horizontal label">{{data.hash}}</div>
			</div>
		</template>
		<template v-if="!data && height">
			<div class="ui large pointing menu">
				<template v-if="height > 0">
					<router-link class="item" :to="'/explore/block/height/' + (height - 1)">Previous</router-link>
				</template>
				<router-link class="right item" :to="'/explore/block/height/' + (height + 1)">Next</router-link>
			</div>
		</template>
		<template v-if="!data && !loading">
			<div class="ui large negative message">
				No such block!
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
					<td>{{data.height}}</td>
				</tr>
				<tr>
					<td class="two wide">Hash</td>
					<td>{{data.hash}}</td>
				</tr>
				<tr>
					<td class="two wide">Previous</td>
					<td>{{data.prev}}</td>
				</tr>
				<tr>
					<td class="two wide">Time</td>
					<td>{{new Date(data.time * 1000).toLocaleString()}}</td>
				</tr>
				<tr>
					<td class="two wide">Time Diff</td>
					<td>{{data.time_diff}}</td>
				</tr>
				<tr>
					<td class="two wide">Space Diff</td>
					<td>{{data.space_diff}}</td>
				</tr>
				<tr>
					<td class="two wide">VDF Iterations</td>
					<td>{{data.vdf_iters}}</td>
				</tr>
				<template v-if="data.tx_base">
					<tr>
						<td class="two wide">TX Base</td>
						<td><router-link :to="'/explore/transaction/' + data.tx_base.id">{{data.tx_base.id}}</router-link></td>
					</tr>
				</template>
				<template v-if="data.proof">
					<tr>
						<td class="two wide">TX Count</td>
						<td>{{data.tx_count}}</td>
					</tr>
					<tr>
						<td class="two wide">K Size</td>
						<td>{{data.proof.ksize}}</td>
					</tr>
					<tr>
						<td class="two wide">Proof Score</td>
						<td>{{data.proof.score}}</td>
					</tr>
					<tr>
						<td class="two wide">Plot ID</td>
						<td>{{data.proof.plot_id}}</td>
					</tr>
					<tr>
						<td class="two wide">Farmer Key</td>
						<td>{{data.proof.farmer_key}}</td>
					</tr>
				</template>
				</tbody>
			</table>
			<table class="ui definition table striped" v-if="data.tx_base">
				<thead>
				<tr>
					<th></th>
					<th>Amount</th>
					<th></th>
					<th>Address</th>
				</tr>
				</thead>
				<tbody>
				<tr v-for="(item, index) in data.tx_base.outputs" :key="index">
					<td class="two wide">Reward[{{index}}]</td>
					<td class="collapsing"><b>{{item.value}}</b></td>
					<td>{{item.symbol}}</td>
					<td><router-link :to="'/explore/address/' + item.address">{{item.address}}</router-link></td>
				</tr>
				</tbody>
			</table>
			<table class="ui definition table striped" v-if="data.tx_list.length">
				<thead>
				<tr>
					<th></th>
					<th>Inputs</th>
					<th>Outputs</th>
					<th>Operations</th>
					<th>Transaction ID</th>
				</tr>
				</thead>
				<tbody>
				<tr v-for="(item, index) in data.tx_list" :key="index">
					<td class="two wide">TX[{{index}}]</td>
					<td>{{item.inputs.length}}</td>
					<td>{{item.outputs.length + item.exec_outputs.length}}</td>
					<td>{{item.execute.length}}</td>
					<td><router-link :to="'/explore/transaction/' + item.id">{{item.id}}</router-link></td>
				</tr>
				</tbody>
			</table>
		</template>
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
					.then(response => {
						if(response.ok) {
							response.json()
								.then(data => {
									for(const op of data.operations) {
										delete op.solution;
									}
									this.loading = false;
									this.data = data;
								});
						} else {
							this.loading = false;
							this.data = null;
						}
					});
			}
		}
	},
	watch: {
		id() {
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
						<template v-if="data.height">
							<router-link :to="'/explore/block/height/' + data.height">{{data.height}}</router-link>
						</template>
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
					<td colspan="2"><router-link :to="'/explore/address/' + data.address">{{data.address}}</router-link></td>
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
			<table class="ui definition table striped" v-if="data.inputs.length">
				<thead>
				<tr>
					<th></th>
					<th>Amount</th>
					<th>Token</th>
					<th>Address</th>
					<th></th>
				</tr>
				</thead>
				<tbody>
				<tr v-for="(item, index) in data.inputs" :key="index">
					<td class="two wide">Input[{{index}}]</td>
					<td class="collapsing"><b>{{item.utxo.value}}</b></td>
					<td>{{item.utxo.symbol}}</td>
					<td><router-link :to="'/explore/address/' + item.utxo.address">{{item.utxo.address}}</router-link></td>
					<td><router-link :to="'/explore/transaction/' + item.prev.txid">Prev</router-link></td>
				</tr>
				</tbody>
			</table>
			<table class="ui definition table striped" v-if="data.outputs.length">
				<thead>
				<tr>
					<th></th>
					<th>Amount</th>
					<th>Token</th>
					<th>Address</th>
					<th></th>
				</tr>
				</thead>
				<tbody>
				<tr v-for="(item, index) in data.outputs" :key="index">
					<td class="two wide">Output[{{index}}]</td>
					<td class="collapsing"><b>{{item.output.value}}</b></td>
					<td>{{item.output.symbol}}</td>
					<td><router-link :to="'/explore/address/' + item.output.address">{{item.output.address}}</router-link></td>
					<td><template v-if="item.spent"><router-link :to="'/explore/transaction/' + item.spent.txid">Next</router-link></template></td>
				</tr>
				</tbody>
			</table>
			<template v-for="(op, index) in data.operations" :key="index">
				<div class="ui segment">
					<div class="ui large label">Operation[{{index}}]</div>
					<div class="ui large label">{{op.__type}}</div>
					<object-table :data="op"></object-table>
				</div>
			</template>
			<template v-if="data.deployed">
				<div class="ui segment">
					<div class="ui large label">{{data.deployed.__type}}</div>
					<object-table :data="data.deployed"></object-table>
				</div>
			</template>
		</template>
		`
})

app.component('address-view', {
	props: {
		address: String
	},
	data() {
		return {
			data: null
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/contract?id=' + this.address)
				.then(response => {
					if(response.ok) {
						response.json()
							.then(data => {
								this.loading = false;
								this.data = data;
							});
					} else {
						this.loading = false;
						this.data = null;
					}
				});
		}
	},
	watch: {
		address() {
			this.update();
		}
	},
	created() {
		this.update();
	},
	template: `
		<div class="ui large labels">
			<div class="ui horizontal label">Address</div>
			<div class="ui horizontal label">{{address}}</div>
		</div>
		<balance-table :address="address" :show_empty="true"></balance-table>
		<template v-if="data">
			<div class="ui segment">
				<div class="ui large label">{{data.__type}}</div>
				<object-table :data="data"></object-table>
			</div>
		</template>
		<address-history-table :address="address" :limit="200" :show_empty="false"></address-history-table>
		`
})

app.component('address-history-table', {
	props: {
		address: String,
		limit: Number,
		show_empty: Boolean
	},
	data() {
		return {
			data: null,
			loading: false,
			timer: null
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/address/history?limit=' + this.limit + '&id=' + this.address)
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.data = data;
				});
		}
	},
	watch: {
		address() {
			this.update();
		},
		limit() {
			this.update();
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 60000);
	},
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<template v-if="!data && loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<table class="ui table striped" v-if="data && (data.length || show_empty)">
			<thead>
			<tr>
				<th>Height</th>
				<th>Type</th>
				<th>Amount</th>
				<th>Token</th>
				<th>Address</th>
				<th>Link</th>
				<th>Time</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data">
				<td><router-link :to="'/explore/block/height/' + item.height">{{item.height}}</router-link></td>
				<td>{{item.type}}</td>
				<td><b>{{item.value}}</b></td>
				<template v-if="item.is_native">
					<td>{{item.symbol}}</td>
				</template>
				<template v-if="!item.is_native">
					<td><router-link :to="'/explore/address/' + item.contract">{{item.symbol}}</router-link></td>
				</template>
				<td><router-link :to="'/explore/address/' + item.address">{{item.address}}</router-link></td>
				<td><router-link :to="'/explore/transaction/' + item.txid">TX</router-link></td>
				<td>{{new Date(item.time * 1000).toLocaleString()}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('object-table', {
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
