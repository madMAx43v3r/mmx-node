
Vue.component('explore-menu', {
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
		<v-tabs class="mb-2">
			<v-tab to="/explore/blocks">{{ $t('explore_menu.blocks') }}</v-tab>
			<v-tab to="/explore/transactions">{{ $t('explore_menu.transactions') }}</v-tab>
			<div class="item" style="flex-grow:1;">
			    <!-- TODO error class -->
				<div :class="{error: !!error}">
					<v-text-field 
						v-model="input" 
						@keyup.enter="submit"
						@click:append="submit" 
						:placeholder="$t('explore_menu.placeholder')"
						append-icon="mdi-database-search-outline"></v-text-field>
				</div>
			</div>			
		</v-tabs>
	`
})

Vue.component('explore-blocks', {
	props: {
		limit: Number
	},
	data() {
		return {
			data: [],
			timer: null,
			loaded: false,
			headers: [
				{ text: this.$t('explore_blocks.height'), value: 'height' },
				{ text: this.$t('explore_blocks.tx'), value: 'tx_count' },
				{ text: this.$t('explore_blocks.k'), value: 'ksize' },
				{ text: this.$t('explore_blocks.score'), value: 'score' },
				{ text: this.$t('explore_blocks.reward'), value: 'reward' },
				{ text: this.$t('explore_blocks.tdiff'), value: 'time_diff' },
				{ text: this.$t('explore_blocks.sdiff'), value: 'space_diff' },
				{ text: this.$t('explore_blocks.hash'), value: 'hash' },
			]
		}
	},
	methods: {
		update() {
			fetch('/wapi/headers?limit=' + this.limit)
				.then(response => response.json())
				.then(data => {
					this.loaded = true;
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
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<v-data-table
			:headers="headers"
			:items="data"
			:loading="!loaded"
			hide-default-footer
			disable-sort
			disable-pagination
			class="elevation-2"
		>
			<template v-slot:item.height="{ item }">
				<router-link :to="'/explore/block/height/' + item.height">{{item.height}}</router-link>
			</template>

			<template v-slot:item.ksize="{ item }">
				{{item.proof ? item.proof.ksize : null}}
			</template>

			<template v-slot:item.score="{ item }">
				{{item.proof ? item.proof.score : null}}
			</template>

			<template v-slot:item.reward="{ item }">
				{{item.reward != null ? item.reward.toFixed(3) : null}}
			</template>			

			<template v-slot:item.hash="{ item }">
				<router-link :to="'/explore/block/hash/' + item.hash">{{item.hash}}</router-link>
			</template>		

		</v-data-table>
		`
})

Vue.component('explore-transactions', {
	props: {
		limit: Number
	},
	data() {
		return {
			data: [],
			timer: null,
			loaded: false,
			headers: [
				{ text: this.$t('explore_transactions.height'), value: 'height' },
				{ text: this.$t('explore_transactions.type'), value: 'type' },
				{ text: this.$t('explore_transactions.fee'), value: 'fee' },
				{ text: this.$t('explore_transactions.n_in'), value: 'inputs.length' },
				{ text: this.$t('explore_transactions.n_out'), value: 'outputs.length' },
				{ text: this.$t('explore_transactions.n_op'), value: 'operations.length' },
				{ text: this.$t('explore_transactions.transaction_id'), value: 'transaction_id' },
			]
		}
	},
	methods: {
		update() {
			fetch('/wapi/transactions?limit=' + this.limit)
				.then(response => response.json())
				.then(data => {
					this.loaded = true;
					this.data = data;
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 10000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<v-data-table
			:headers="headers"
			:items="data"
			:loading="!loaded"
			hide-default-footer
			disable-sort			
			disable-pagination
			class="elevation-2"
		>
			<template v-slot:item.height="{ item }">
				<router-link :to="'/explore/block/height/' + item.height">{{item.height}}</router-link>
			</template>

			<template v-slot:item.type="{ item }">
				{{item.note ? item.note : ""}}
			</template>

			<template v-slot:item.fee="{ item }">
				<b>{{item.fee.value}}</b>
			</template>

			<template v-slot:item.transaction_id="{ item }">
				<router-link :to="'/explore/transaction/' + item.id">{{item.id}}</router-link>
			</template>

		</v-data-table>
		`
})

Vue.component('block-view', {
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
		<div>

			<v-toolbar dense flat class="pa-0 no-padding">
				<template v-if="data">
					<v-btn :to="'/explore/block/hash/' + data.prev"><v-icon>mdi-arrow-left</v-icon>{{ $t('block_view.previous') }}</v-btn>
					<v-spacer></v-spacer>
					<v-btn :to="'/explore/block/height/' + (data.height + 1)">{{ $t('block_view.next') }}<v-icon>mdi-arrow-right</v-icon></v-btn>
				</template>

				<template v-if="!data && height">
					<div v-if="height > 0">
						<v-btn :to="'/explore/block/height/' + (height - 1)"><v-icon>mdi-arrow-left</v-icon>{{ $t('block_view.previous') }}</v-btn>
					</div>
					<v-spacer></v-spacer>
					<v-btn :to="'/explore/block/height/' + (height + 1)">{{ $t('block_view.next') }}<v-icon>mdi-arrow-right</v-icon></v-btn>
				</template>
				<v-progress-linear :active="loading" indeterminate absolute top></v-progress-linear>
			</v-toolbar>

			<template v-if="data">
				<v-chip label>{{ $t('block_view.block') }}</v-chip>
				<v-chip label>{{ data.height }}</v-chip>
				<v-chip label>{{ data.hash }}</v-chip>
			</template>

			<template v-if="!data && !loading">							
				<v-alert
					border="right"
					colored-border
					type="warning"
					elevation="2"
				>
					{{ $t('block_view.no_such_block') }}
				</v-alert>
			</template>

			<template v-if="data">
				<v-card class="my-2">
					<v-simple-table class="rounded">
						<template v-slot:default>
							<tbody>
								<tr>
									<td class="two wide">{{ $t('block_view.height') }}</td>
									<td>{{data.height}}</td>
								</tr>
								<tr>
									<td class="two wide">{{ $t('block_view.hash') }}</td>
									<td>{{data.hash}}</td>
								</tr>
								<tr>
									<td class="two wide">{{ $t('block_view.previous') }}</td>
									<td>{{data.prev}}</td>
								</tr>
								<tr>
									<td class="two wide">{{ $t('block_view.time') }}</td>
									<td>{{new Date(data.time * 1000).toLocaleString()}}</td>
								</tr>
								<tr>
									<td class="two wide">{{ $t('block_view.time_diff') }}</td>
									<td>{{data.time_diff}}</td>
								</tr>
								<tr>
									<td class="two wide">{{ $t('block_view.space_diff') }}</td>
									<td>{{data.space_diff}}</td>
								</tr>
								<tr>
									<td class="two wide">{{ $t('block_view.vdf_iterations') }}</td>
									<td>{{data.vdf_iters}}</td>
								</tr>
								<template v-if="data.tx_base">
									<tr>
										<td class="two wide">{{ $t('block_view.tx_base') }}</td>
										<td><router-link :to="'/explore/transaction/' + data.tx_base.id">{{data.tx_base.id}}</router-link></td>
									</tr>
								</template>
								<template v-if="data.proof">
									<tr>
										<td class="two wide">{{ $t('block_view.tx_count') }}</td>
										<td>{{data.tx_count}}</td>
									</tr>
									<tr>
										<td class="two wide">{{ $t('block_view.k_size') }}</td>
										<td>{{data.proof.ksize}}</td>
									</tr>
									<tr>
										<td class="two wide">{{ $t('block_view.proof_score') }}</td>
										<td>{{data.proof.score}}</td>
									</tr>
									<tr>
										<td class="two wide">{{ $t('block_view.plot_id') }}</td>
										<td>{{data.proof.plot_id}}</td>
									</tr>
									<tr>
										<td class="two wide">{{ $t('block_view.farmer_key') }}</td>
										<td>{{data.proof.farmer_key}}</td>
									</tr>
								</template>
							</tbody>
						</template>
					</v-simple-table>
				</v-card>

				<v-card class="my-2">
					<v-simple-table v-if="data.tx_base">
						<template v-slot:default>
							<thead>
								<tr>
									<th></th>
									<th>{{ $t('block_view.amount') }}</th>
									<th></th>
									<th>{{ $t('block_view.address') }}</th>
								</tr>
							</thead>
							<tbody>
								<tr v-for="(item, index) in data.tx_base.outputs" :key="index">
									<td class="two wide">{{ $t('block_view.reward') }}[{{index}}]</td>
									<td class="collapsing"><b>{{item.value}}</b></td>
									<td>{{item.symbol}}</td>
									<td><router-link :to="'/explore/address/' + item.address">{{item.address}}</router-link></td>
								</tr>
							</tbody>
						</template>
					</v-simple-table>
				</v-card>

				<v-card class="my-2">
					<v-simple-table v-if="data.tx_list.length">
						<thead>
							<tr>
								<th></th>
								<th>{{ $t('block_view.inputs') }}</th>
								<th>{{ $t('block_view.outputs') }}</th>
								<th>{{ $t('block_view.operations') }}</th>
								<th>{{ $t('block_view.transaction_id') }}</th>
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
					</v-simple-table>
				</v-card>
			</template>
		</div>
		`
})

Vue.component('transaction-view', {
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
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<div class="ui large labels">
			<div class="ui horizontal label">{{ $t('transaction_view.transaction') }}</div>
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
					<td class="two wide">{{ $t('transaction_view.height') }}</td>
					<td colspan="2">
						<template v-if="data.height">
							<router-link :to="'/explore/block/height/' + data.height">{{data.height}}</router-link>
						</template>
						<template v-if="!data.height"><i>pending</i></template>
					</td>
				</tr>
				<tr v-if="data.confirm">
					<td class="two wide">{{ $t('transaction_view.confirmed') }}</td>
					<td colspan="2">{{data.confirm}}</td>
				</tr>
				<tr v-if="data.expires != 4294967295">
					<td class="two wide">{{ $t('transaction_view.expires') }}</td>
					<td colspan="2">{{data.expires}}</td>
				</tr>
				<tr>
					<td class="two wide">{{ $t('transaction_view.note') }}</td>
					<td colspan="2">{{data.note}}</td>
				</tr>
				<tr v-if="data.time">
					<td class="two wide">{{ $t('transaction_view.time') }}</td>
					<td colspan="2">{{new Date(data.time * 1000).toLocaleString()}}</td>
				</tr>
				<tr v-if="data.deployed">
					<td class="two wide">{{ $t('transaction_view.address') }}</td>
					<td colspan="2"><router-link :to="'/explore/address/' + data.address">{{data.address}}</router-link></td>
				</tr>
				<tr v-if="data.sender">
					<td class="two wide">{{ $t('transaction_view.sender') }}</td>
					<td colspan="2"><router-link :to="'/explore/address/' + data.sender">{{data.sender}}</router-link></td>
				</tr>
				<tr>
					<td class="two wide">{{ $t('transaction_view.cost') }}</td>
					<td class="collapsing"><b>{{data.cost.value}}</b></td>
					<td>MMX</td>
				</tr>
				<tr>
					<td class="two wide">{{ $t('transaction_view.fee') }}</td>
					<td class="collapsing"><b>{{data.fee.value}}</b></td>
					<td>MMX</td>
				</tr>
				</tbody>
			</table>
			<table class="ui compact definition table striped" v-if="data.inputs.length">
				<thead>
				<tr>
					<th></th>
					<th>{{ $t('transaction_view.amount') }}</th>
					<th>{{ $t('transaction_view.token') }}</th>
					<th>{{ $t('transaction_view.address') }}</th>
				</tr>
				</thead>
				<tbody>
				<tr v-for="(item, index) in data.inputs" :key="index">
					<td class="two wide">{{ $t('transaction_view.input') }}[{{index}}]</td>
					<td class="collapsing"><b>{{item.value}}</b></td>
					<template v-if="item.is_native">
						<td>{{item.symbol}}</td>
					</template>
					<template v-if="!item.is_native">
						<td><router-link :to="'/explore/address/' + item.contract">{{item.is_nft ? "[NFT]" : item.symbol}}</router-link></td>
					</template>
					<td><router-link :to="'/explore/address/' + item.address">{{item.address}}</router-link></td>
				</tr>
				</tbody>
			</table>
			<table class="ui compact definition table striped" v-if="data.outputs.length">
				<thead>
				<tr>
					<th></th>
					<th>{{ $t('transaction_view.amount') }}</th>
					<th>{{ $t('transaction_view.token') }}</th>
					<th>{{ $t('transaction_view.address') }}</th>
				</tr>
				</thead>
				<tbody>
				<tr v-for="(item, index) in data.outputs" :key="index">
					<td class="two wide">{{ $t('transaction_view.output') }}[{{index}}]</td>
					<td class="collapsing"><b>{{item.value}}</b></td>
					<template v-if="item.is_native">
						<td>{{item.symbol}}</td>
					</template>
					<template v-if="!item.is_native">
						<td><router-link :to="'/explore/address/' + item.contract">{{item.is_nft ? "[NFT]" : item.symbol}}</router-link></td>
					</template>
					<td><router-link :to="'/explore/address/' + item.address">{{item.address}}</router-link></td>
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

Vue.component('address-view', {
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
	<div>
		<v-chip label>Address</v-chip>
		<v-chip label>{{ address }}</v-chip>
		</v-card-title-header>

		<balance-table :address="address" :show_empty="true"></balance-table>

		<template v-if="data">
			<div class="ui segment">
				<div class="ui large label">{{data.__type}}</div>
				<object-table :data="data"></object-table>
			</div>
		</template>

		<address-history-table :address="address" :limit="200" :show_empty="false" class="my-2"></address-history-table>
	</div>
		`
})

Vue.component('address-history-table', {
	props: {
		address: String,
		limit: Number,
		show_empty: Boolean
	},
	data() {
		return {
			data: [],
			loaded: false,
			timer: null,
			headers: [
				{ text: this.$t('address_history_table.height'), value: 'height'},
				{ text: this.$t('address_history_table.type'), value: 'type'},
				{ text: this.$t('address_history_table.amount'), value: 'amount'},
				{ text: this.$t('address_history_table.token'), value: 'token'},
				{ text: this.$t('address_history_table.address'), value: 'address'},
				{ text: this.$t('address_history_table.link'), value: 'link'},
				{ text: this.$t('address_history_table.time'), value: 'time'},
			],			
		}
	},
	methods: {
		update() {
			fetch('/wapi/address/history?limit=' + this.limit + '&id=' + this.address)
				.then(response => response.json())
				.then(data => {
					this.loaded = true;
					this.data = data;
				}).catch(() => {
					this.loaded = true;
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
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<v-data-table
			:headers="headers"
			:items="data"
			:loading="!loaded"
			hide-default-footer
			disable-sort
			disable-pagination
			class="elevation-2"
			v-if="!loaded || loaded && (data.length || show_empty)"
		>
			<template v-slot:item.height="{ item }">
				<router-link :to="'/explore/block/height/' + item.height">{{item.height}}</router-link>
			</template>

			<template v-slot:item.amount="{ item }">
				<b>{{ item.value }}</b>
			</template>

			<template v-slot:item.token="{ item }">
				<template v-if="item.is_native">
					{{item.symbol}}
				</template>
				<template v-else>
					<router-link :to="'/explore/address/' + item.contract">{{item.is_nft ? "[NFT]" : item.symbol}}</router-link>
				</template>
			</template>


			<template v-slot:item.address="{ item }">
				<router-link :to="'/explore/address/' + item.address">{{item.address}}</router-link>
			</template>

			<template v-slot:item.link="{ item }">
				<router-link :to="'/explore/transaction/' + item.txid">TX</router-link>
			</template>

			<template v-slot:item.time="{ item }">
				{{ new Date(item.time * 1000).toLocaleString() }}
			</template>

		</v-data-table>
		`
})

Vue.component('object-table', {
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
