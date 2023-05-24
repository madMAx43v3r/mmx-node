
Vue.component('swap-menu', {
	props: {
		loading: false
	},
	data() {
		return {
			tokens: [],
			token: null,
			currency: null
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/wallet/tokens')
				.then(response => response.json())
				.then(data => {
					this.tokens = data;
					this.loading = false;
				});
		},
		submit() {
			this.$router.push('/swap/market/' + this.token + '/' + this.currency).catch(()=>{});
		}
	},
	created() {
		this.update();
		this.submit();
	},
	watch: {
		token() {
			this.submit();
		},
		currency() {
			this.submit();
		}
	},
	computed: {
		selectItems() {
			var result = [];
			
			result.push({ text: this.$t('market_menu.anything'), value: null});

			this.tokens.map(item => {
				var text = item.symbol;
				if(item.currency != MMX_ADDR) {
					text += ` - [${item.currency}]`;
				}
				result.push(
					{
						text: text,
						value: item.currency
					}
				);
			});

			return result;
		}
	},
	template: `
		<div>
			<v-select
				v-model="token"
				:items="selectItems"
				:label="$t('common.token')"
				item-text="text"
				item-value="value"
			></v-select>

			<v-select
				v-model="currency"
				:items="selectItems"
				:label="$t('common.currency')"
				item-text="text"
				item-value="value"
			></v-select>
		</div>
		`
})

Vue.component('swap-list', {
	props: {
		token: null,
		currency: null,
		limit: Number
	},
	data() {
		return {
			data: null,
			offer: {},
			tokens: null,
			timer: null,
			loading: false,
		}
	},
	methods: {
		update() {
			if(this.tokens) {
				this.loading = true;
				let query = '/wapi/swap/list?limit=' + this.limit;
				if(this.token) {
					query += '&token=' + this.token;
				}
				if(this.currency) {
					query += '&currency=' + this.currency;
				}
				fetch(query)
					.then(response => response.json())
					.then(data => {
						this.loading = false;
						this.data = data;
					});
			} else {
				fetch('/wapi/wallet/tokens')
					.then(response => response.json())
					.then(data => {
						this.tokens = new Set();
						for(const token of data) {
							this.tokens.add(token.currency);
						}
						this.update();
					});
			}
		}
	},
	watch: {
		token() {
			this.update();
		},
		currency() {
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
		<div>
			<v-card>
				<div v-if="!data && loading">
					<v-progress-linear indeterminate absolute top></v-progress-linear>
					<v-skeleton-loader type="table-row-divider@6"/>
				</div>

				<template v-if="data">
					<v-simple-table>
						<thead>
							<tr>
								<th>{{ $t('common.name') }}</th>
								<th>{{ $t('common.token') }}</th>
								<th>{{ $t('common.currency') }}</th>
								<th>{{ $t('common.price') }}</th>
								<th>{{ $t('swap.pool_balance') }}</th>
								<th>{{ $t('swap.pool_balance') }}</th>
								<th></th>
							</tr>
						</thead>
						<tbody>
							<tr v-for="item in data" :key="item.address">
								<td>{{item.name}}</td>
								<td>
									<template v-if="item.symbols[0] == 'MMX'">MMX</template>
									<template v-else>
										<router-link :to="'/explore/address/' + item.tokens[0]">
											<template v-if="tokens.has(item.tokens[0])">{{item.symbols[0]}}</template>
											<template v-else>{{item.symbols[0]}}?</template>
										</router-link>
									</template>
								</td>
								<td>
									<template v-if="item.symbols[1] == 'MMX'">MMX</template>
									<template v-else>
										<router-link :to="'/explore/address/' + item.tokens[1]">
											<template v-if="tokens.has(item.tokens[1])">{{item.symbols[1]}}</template>
											<template v-else>{{item.symbols[1]}}?</template>
										</router-link>
									</template>
								</td>
								<td><b>{{ item.price ? parseFloat( (item.price).toPrecision(6) ) : "N/A" }}</b>&nbsp; {{item.symbols[1]}} / {{item.symbols[0]}}</td>
								<td><b>{{ parseFloat( (item.balance[0].value).toPrecision(6) ) }}</b>&nbsp; {{item.symbols[0]}}</td>
								<td><b>{{ parseFloat( (item.balance[1].value).toPrecision(6) ) }}</b>&nbsp; {{item.symbols[1]}}</td>
								<td>
									<v-btn :to="'/swap/trade/' + item.address" outlined text>{{ $t('swap.swap') }}</v-btn>
								</td>
							</tr>
						</tbody>
					</v-simple-table>
				</template>
			</v-card>
		</div>
		`
})

Vue.component('swap-sub-menu', {
	props: {
		address: String
	},
	template: `
		<v-tabs>
			<v-tab :to="'/swap/trade/' + address">{{ $t('swap.trade') }}</v-tab>
			<v-tab :to="'/swap/history/' + address">{{ $t('swap.history') }}</v-tab>
			<v-tab :to="'/swap/liquid/' + address">{{ $t('swap.my_liquidity') }}</v-tab>
			<v-tab :to="'/swap/pool/' + address">{{ $t('swap.pool_state') }}</v-tab>
		</v-tabs>
	`
})

Vue.component('swap-info', {
	props: {
		address: String
	},
	data() {
		return {
			data: null,
			timer: null,
			loading: true,
		}
	},
	methods: {
		update() {
			fetch('/wapi/swap/info?id=' + this.address)
				.then(response => response.json())
				.then(data => {
					this.data = data;
					this.loading = false;
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 5000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<div>
			<template v-if="data">
				<v-chip label>{{ $t('swap.swap') }}</v-chip>
				<v-chip label>{{data.price ? parseFloat((data.price).toPrecision(6)) : "N/A"}}&nbsp; {{data.symbols[1]}} / {{data.symbols[0]}}</v-chip>
				<v-chip label>{{data.address}}</v-chip>
			</template>
			
			<v-card class="my-2">
				<div v-if="!data && loading">
					<v-progress-linear indeterminate></v-progress-linear>
					<v-skeleton-loader type="table-row-divider@2"/>
				</div>

				<v-simple-table v-if="data">
					<thead>
						<tr>
							<th></th>
							<th>{{ $t('swap.pool_balance') }}</th>
							<th>{{ $t('swap.volume') }} ({{ $t('swap.24h') }})</th>
							<th>{{ $t('swap.volume') }} ({{ $t('swap.7d') }})</th>
							<th>{{ $t('swap.apy') }} ({{ $t('swap.24h') }})</th>
							<th>{{ $t('swap.apy') }} ({{ $t('swap.7d') }})</th>
						</tr>
					</thead>
					<tbody>
						<template v-for="i in [0, 1]">
							<tr>
								<td class="key-cell">{{i == 0 ? $t('common.token') : $t('common.currency') }}</td>
								<td>
									<b>{{ parseFloat( (data.balance[i].value).toPrecision(6) ) }}</b>
									<template v-if="data.symbols[i] == 'MMX'">MMX</template>
									<template v-else>
										<router-link :to="'/explore/address/' + data.tokens[i]">{{data.symbols[i]}}</router-link>
									</template>
								</td>
								<td><b>{{parseFloat(data.volume_1d[i].value.toPrecision(6))}}</b> {{data.symbols[i]}}</td>
								<td><b>{{parseFloat(data.volume_7d[i].value.toPrecision(6))}}</b> {{data.symbols[i]}}</td>
								<td>{{(data.avg_apy_1d[i] * 100).toFixed(2)}} %</td>
								<td>{{(data.avg_apy_7d[i] * 100).toFixed(2)}} %</td>
							</tr>
						</template>
					</tbody>
				</v-simple-table>
			</v-card>
		</div>
	`
})

Vue.component('swap-pool-info', {
	props: {
		address: String
	},
	data() {
		return {
			data: null,
			timer: null,
			loading: true,
		}
	},
	methods: {
		update() {
			fetch('/wapi/swap/info?id=' + this.address)
				.then(response => response.json())
				.then(data => {
					this.data = data;
					this.loading = false;
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
		<v-card>
			<div v-if="!data && loading">
				<v-progress-linear indeterminate></v-progress-linear>
				<v-skeleton-loader type="table-row-divider@10"/>
			</div>

			<v-simple-table v-if="data">
				<thead>
					<tr>
						<th>{{ $t('swap.fee_level') }}</th>
						<th>{{ $t('common.balance') }}</th>
						<th>{{ $t('common.balance') }}</th>
						<th>Price</th>
						<th>{{ $t('swap.user_total') }}</th>
						<th>{{ $t('swap.user_total') }}</th>
					</tr>
				</thead>
				<tbody>
					<template v-for="(fee_rate, k) in data.fee_rates">
						<tr>
							<td class="key-cell">{{fee_rate * 100}} %</td>
							<template v-for="i in [0, 1]">
								<td><b>{{ parseFloat( (data.pools[k].balance[i].value).toPrecision(6) ) }}</b> {{data.symbols[i]}}</td>
							</template>
							<td>{{data.pools[k].price != null ? parseFloat((data.pools[k].price).toPrecision(6)) : "N/A"}}</td>
							<template v-for="i in [0, 1]">
								<td><b>{{ parseFloat( (data.pools[k].user_total[i].value).toPrecision(6) ) }}</b> {{data.symbols[i]}}</td>
							</template>
						</tr>
					</template>
				</tbody>
			</v-simple-table>
		</v-card>
	`
})

Vue.component('swap-user-info', {
	props: {
		address: String,
		user: String
	},
	emits: [
		"user-update"
	],
	data() {
		return {
			data: null,
			timer: null,
			loading: true
		}
	},
	methods: {
		update() {
			fetch('/wapi/swap/user_info?id=' + this.address + "&user=" + this.user)
				.then(response => response.json())
				.then(data => {
					this.data = data;
					this.loading = false;
					this.$emit('user-update', data);
				});
		}
	},
	watch: {
		user(value) {
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
		<v-card>
			<div v-if="!data && loading">
				<v-progress-linear indeterminate></v-progress-linear>
				<v-skeleton-loader type="table-row-divider@2"/>
			</div>

			<v-simple-table v-if="data">
				<thead>
					<tr>
						<th>{{ $t('swap.my_balance') }}</th>
						<th>{{ $t('swap.my_liquidity') }}</th>
						<th>{{ $t('swap.fees_earned') }}</th>
						<th>{{ $t('swap.fee_level') }}</th>
						<th>{{ $t('swap.unlock_height') }}</th>
					</tr>
				</thead>
				<tbody>
					<template v-for="i in [0, 1]">
						<tr>
							<td><b>{{ parseFloat( (data.balance[i].value).toPrecision(6) ) }}</b> {{data.symbols[i]}}</td>
							<td><b>{{ parseFloat( (data.equivalent_liquidity[i].value).toPrecision(6) ) }}</b> {{data.symbols[i]}}</td>
							<td><b>{{ parseFloat( (data.fees_earned[i].value).toPrecision(6) ) }}</b> {{data.symbols[i]}}</td>
							<td><template v-if="data.pool_idx >= 0">{{(data.swap.fee_rates[data.pool_idx] * 100).toFixed(2)}} %</template></td>
							<td>{{data.unlock_height}}</td>
						</tr>
					</template>
				</tbody>
			</v-simple-table>
		</v-card>
	`
})

Vue.component('swap-history', {
	props: {
		address: String,
		limit: Number
	},
	data() {
		return {
			data: [],
			color_map: new Map([
				["BUY", "green"],
				["SELL", "red"],
			]),
			loading: true,
		}
	},
	computed: {
		headers() {
			return [
				{ text: this.$t('common.height'), value: 'height' },
				{ text: this.$t('common.type'), value: 'type' },
				{ text: this.$t('common.amount'), value: 'value' },
				{ text: this.$t('common.symbol'), value: 'symbol' },
				{ text: this.$t('common.user'), value: 'user' },
				{ text: this.$t('common.link'), value: 'txid' },
				{ text: this.$t('common.time'), value: 'time' },
			]
		}
	},
	methods: {
		update() {
			fetch('/wapi/swap/history?id=' + this.address + '&limit=' + this.limit)
				.then(response => response.json())
				.then(data => {
					this.data = data;
					this.loading = false;
				});
		},
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 10000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<div>
			<v-data-table
				:headers="headers"
				:items="data"
				:loading="loading"
				hide-default-footer
				disable-sort
				disable-pagination
				class="elevation-2"
			>
				<template v-slot:progress>
					<v-progress-linear indeterminate></v-progress-linear>
					<v-skeleton-loader type="table-row-divider@6" />
				</template>
				
				<template v-slot:item.type="{ item }">
					<div :class="color_map.get(item.type) + '--text'">{{item.type}}</div>
				</template>
				
				<template v-slot:item.value="{ item }">
					<b>{{item.value}}</b>
				</template>
				
				<template v-slot:item.user="{ item }">
					<router-link :to="'/explore/address/' + item.user">{{item.user}}</router-link>
				</template>
				
				<template v-slot:item.txid="{ item }">
					<router-link :to="'/explore/transaction/' + item.txid">TX</router-link>
				</template>
	
				<template v-slot:item.time="{ item }">
					{{new Date(item.time * 1000).toLocaleString()}}
				</template>
			</v-data-table>
		</div>
		`
})

Vue.component('swap-trade', {
	props: {
		address: String,
		wallet: null
	},
	data() {
		return {
			data: null,
			buy_fee: null,
			buy_price: null,
			buy_amount: null,
			buy_balance: null,
			buy_estimate: null,
			buy_num_iter: 20,
			buy_slippage: 0.98,
			buy_tx_fee: null,
			sell_fee: null,
			sell_price: null,
			sell_amount: null,
			sell_balance: null,
			sell_estimate: null,
			sell_num_iter: 20,
			sell_slippage: 0.98,
			sell_tx_fee: null,
			result: null,
			error: null,
			num_iter_items: [1, 5, 10, 20, 50, 200],
			slippage_items: [
				{text: "0.5 %", value: 0.995},
				{text: "1 %", value: 0.99},
				{text: "2 %", value: 0.98},
				{text: "5 %", value: 0.95},
				{text: "10 %", value: 0.9},
			],
		}
	},
	methods: {
		update() {
			fetch('/wapi/swap/info?id=' + this.address)
				.then(response => response.json())
				.then(data => {
					this.data = data;
					this.update_wallet();
				});
		},
		update_wallet() {
			fetch('/wapi/wallet/balance?index=' + this.wallet + '&currency=' + this.data.tokens[0])
				.then(response => response.json())
				.then(data => this.sell_balance = data ? data.spendable : 0);
			fetch('/wapi/wallet/balance?index=' + this.wallet + '&currency=' + this.data.tokens[1])
				.then(response => response.json())
				.then(data => this.buy_balance = data ? data.spendable : 0);
		},
		submit(index, amount, min_trade, num_iter, fee_ratio) {
			const req = {};
			req.wallet = this.wallet;
			req.address = this.address;
			req.index = index;
			req.amount = amount;
			req.min_trade = min_trade;
			req.num_iter = num_iter;
			req.options = {};
			if(fee_ratio) {
				req.options.fee_ratio = fee_ratio;
			}
			fetch('/wapi/wallet/swap/trade', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						response.json().then(data => {
							this.result = data;
							this.error = null;
						});
					} else {
						response.text().then(data => {
							this.result = null;
							this.error = data;
						});
					}
				});
			this.buy_amount = null;
			this.sell_amount = null;
		},
		submit_buy() {
			this.submit(1, this.buy_amount, this.buy_estimate * this.buy_slippage, this.buy_num_iter, this.buy_tx_fee);
		},
		submit_sell() {
			this.submit(0, this.sell_amount, this.sell_estimate * this.sell_slippage, this.sell_num_iter, this.sell_tx_fee);
		},
		update_buy_estimate() {
			this.buy_fee = null;
			this.buy_price = null;
			this.buy_estimate = null;
			if(this.buy_amount > 0) {
				fetch('/wapi/swap/trade_estimate?id=' + this.address + '&index=1&amount=' + this.buy_amount + '&iters=' + this.buy_num_iter)
					.then(response => response.json())
					.then(data => {
						this.buy_fee = data.fee_percent.toFixed(2);
						this.buy_price = parseFloat((1 / data.avg_price).toPrecision(6));
						this.buy_estimate = data.trade.value;
					});
			}
		},
		update_sell_estimate() {
			this.sell_fee = null;
			this.sell_price = null;
			this.sell_estimate = null;
			if(this.sell_amount > 0) {
				fetch('/wapi/swap/trade_estimate?id=' + this.address + '&index=0&amount=' + this.sell_amount + '&iters=' + this.sell_num_iter)
					.then(response => response.json())
					.then(data => {
						this.sell_fee = data.fee_percent.toFixed(2);
						this.sell_price = parseFloat(data.avg_price.toPrecision(6));
						this.sell_estimate = data.trade.value;
					});
			}
		}
	},
	watch: {
		wallet() {
			if(this.data) {
				this.update_wallet();
			}
		},
		buy_amount() {
			this.update_buy_estimate();
		},
		buy_num_iter() {
			this.update_buy_estimate();
		},
		sell_amount() {
			this.update_sell_estimate();
		},
		sell_num_iter() {
			this.update_sell_estimate();
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 30000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<div>
			<v-row v-if="data">
				<v-col>
					<v-card>
						<v-toolbar color="green lighten-1" elevation="1" dense></v-toolbar>
						<v-card-text>
							<v-row>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="buy_balance"
										:label="$t('swap.wallet_ballance')"
										:suffix="data.symbols[1]" disabled>
									</v-text-field>
								</v-col>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="buy_amount"
										:label="$t('swap.buy_amount')"
										:suffix="data.symbols[1]">
									</v-text-field>
								</v-col>
							</v-row>
							<v-row>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="buy_fee"
										:label="$t('swap.trade_fee_estimated')"
										suffix="%" disabled>
									</v-text-field>
								</v-col>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="buy_estimate"
										:label="$t('swap.you_receive_estimated')"
										:suffix="data.symbols[0]" disabled>
									</v-text-field>
								</v-col>
							</v-row>
							<v-row>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="buy_price"
										label="Trade Price (average, including fee)"
										:suffix="data.symbols[1] + ' / ' + data.symbols[0]" disabled>
									</v-text-field>
								</v-col>
							</v-row>
							<v-row>
								<v-col>
									<v-select
										v-model="buy_num_iter"
										:items="num_iter_items"
										label="Trade Iterations"
									></v-select>
								</v-col>
								<v-col>
									<v-select
										v-model="buy_slippage"
										:items="slippage_items"
										label="Max Slippage"
									></v-select>
								</v-col>
								<v-col>
									<tx-fee-select @update-value="value => this.buy_tx_fee = value"></tx-fee-select>
								</v-col>
							</v-row>
						</v-card-text>
						<v-card-actions class="justify-end">
							<v-btn @click="update_buy_estimate()" :disabled="!(buy_amount > 0)">Update</v-btn>
							<v-btn color="green lighten-1" @click="submit_buy()" :disabled="!(buy_amount > 0) || !(buy_estimate > 0)">{{ $t('swap.buy') }}</v-btn>
						</v-card-actions>
					</v-card>
				</v-col>
				<v-col>
					<v-card>
						<v-toolbar color="red lighten-1" elevation="1" dense></v-toolbar>
						<v-card-text>
							<v-row>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="sell_balance"
										:label="$t('swap.wallet_ballance')"
										:suffix="data.symbols[0]" disabled>
									</v-text-field>
								</v-col>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="sell_amount"
										:label="$t('swap.sell_amount')"
										:suffix="data.symbols[0]">
									</v-text-field>
								</v-col>
							</v-row>
							<v-row>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="sell_fee"
										:label="$t('swap.trade_fee_estimated')"
										suffix="%" disabled>
									</v-text-field>
								</v-col>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="sell_estimate"
										:label="$t('swap.you_receive_estimated')"
										:suffix="data.symbols[1]" disabled>
									</v-text-field>
								</v-col>
							</v-row>
							<v-row>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="sell_price"
										label="Trade Price (average, including fee)"
										:suffix="data.symbols[1] + ' / ' + data.symbols[0]" disabled>
									</v-text-field>
								</v-col>
							</v-row>
							<v-row>
								<v-col>
									<v-select
										v-model="sell_num_iter"
										:items="num_iter_items"
										label="Trade Iterations"
									></v-select>
								</v-col>
								<v-col>
									<v-select
										v-model="sell_slippage"
										:items="slippage_items"
										label="Max Slippage"
									></v-select>
								</v-col>
								<v-col>
									<tx-fee-select @update-value="value => this.sell_tx_fee = value"></tx-fee-select>
								</v-col>
							</v-row>
						</v-card-text>
						<v-card-actions class="justify-end">
							<v-btn @click="update_sell_estimate()" :disabled="!(sell_amount > 0)">Update</v-btn>
							<v-btn color="red lighten-1" @click="submit_sell()" :disabled="!(sell_amount > 0) || !(sell_estimate > 0)">{{ $t('swap.sell') }}</v-btn>
						</v-card-actions>
					</v-card>
				</v-col>
			</v-row>
			
			<v-alert
				border="left"
				colored-border
				type="success"
				elevation="2"
				v-if="result"
				class="my-2"
			>
				{{ $t('common.transaction_has_been_sent') }}: <router-link :to="'/explore/transaction/' + result.id">{{result.id}}</router-link>
			</v-alert>

			<v-alert
				border="left"
				colored-border
				type="error"
				elevation="2"
				v-if="error"
				class="my-2"
			>
				{{ $t('common.failed_with') }}: <b>{{error}}</b>
			</v-alert>
		</div>
	`
})

Vue.component('swap-liquid', {
	props: {
		address: String,
		wallet: null
	},
	data() {
		return {
			data: null,
			paid: false,
			user: null,
			user_address: null,
			pool_idx: -1,
			amount_0: null,
			amount_1: null,
			price: null,
			result: null,
			error: null,
			loading: true,
		}
	},
	methods: {
		update() {
			if(this.wallet != null) {
				this.update_user();
			}
			fetch('/wapi/swap/info?id=' + this.address)
				.then(response => response.json())
				.then(data => {
					this.data = data;
					this.loading = false;
				});
		},
		update_user() {
			fetch('/wapi/wallet/address?index=' + this.wallet)
				.then(response => response.json())
				.then(data => this.user_address = data[0]);
		},
		update_price() {
			if(this.amount_0 && this.amount_1) {
				this.price = (this.amount_1 / this.amount_0).toPrecision(6);
			} else {
				this.price = null;
			}
		},
		match() {
			if(this.amount_0 && !this.amount_1) {
				this.amount_1 = parseFloat((this.amount_0 * this.data.price).toFixed(this.data.decimals[1]));
			} else if(!this.amount_0 && this.amount_1) {
				this.amount_0 = parseFloat((this.amount_1 / this.data.price).toFixed(this.data.decimals[0]));
			}
		},
		on_response(response, type) {
			if(response.ok) {
				response.json().then(data => {
					this.result = data;
					this.error = null;
					if(type == "payout") {
						this.paid = true;
					}
				});
			} else {
				response.text().then(data => {
					this.result = null;
					this.error = data;
				});
			}
		},
		submit(mode) {
			const req = {};
			req.index = this.wallet;
			req.address = this.address;
			req.pool_idx = this.pool_idx;
			req.amount = [this.amount_0, this.amount_1];
			fetch('/wapi/wallet/swap/' + (mode ? "add" : "rem") + "_liquid", {body: JSON.stringify(req), method: "post"})
				.then(response => this.on_response(response));
			this.amount_0 = null;
			this.amount_1 = null;
		},
		payout() {
			const req = {};
			req.index = this.wallet;
			req.address = this.address;
			fetch('/wapi/wallet/swap/payout', {body: JSON.stringify(req), method: "post"})
				.then(response => this.on_response(response, "payout"));
		},
		switch_pool() {
			const req = {};
			req.index = this.wallet;
			req.address = this.address;
			req.pool_idx = this.pool_idx;
			fetch('/wapi/wallet/swap/switch_pool', {body: JSON.stringify(req), method: "post"})
				.then(response => this.on_response(response, "switch_pool"));
		},
		rem_all_liquid() {
			const req = {};
			req.index = this.wallet;
			req.address = this.address;
			fetch('/wapi/wallet/swap/rem_all_liquid', {body: JSON.stringify(req), method: "post"})
				.then(response => this.on_response(response, "rem_all_liquid"));
		}
	},
	watch: {
		data(value) {
			var fee_rates = [];
			for(let i = 0; i < value.fee_rates.length; ++i) {
				fee_rates.push({text: (value.fee_rates[i] * 100).toFixed(2) + " %", value: i});
			}
			this.fee_rates = fee_rates;
		},
		user(value) {
			if(value && this.pool_idx < 0 && value.pool_idx >= 0) {
				this.pool_idx = value.pool_idx;
			}
		},
		wallet() {
			this.pool_idx = -1;
			this.update_user();
		},
		amount_0() {
			this.update_price();
		},
		amount_1() {
			this.update_price();
		}
	},
	computed: {
		disable_add_rem() {
			return !(this.amount_0 || this.amount_1) || this.pool_idx < 0;
		},
		disable_add() {
			return (this.user.pool_idx >= 0 && this.pool_idx != this.user.pool_idx
					&& (this.user.balance[0].amount != 0 || this.user.balance[1].amount != 0));
		},
		disable_payout() {
			return !this.user || this.paid || !(this.user.fees_earned[0].amount != 0 || this.user.fees_earned[1].amount != 0);
		},
		disable_switch() {
			return this.pool_idx < 0 || this.user.pool_idx < 0 || this.pool_idx == this.user.pool_idx
				|| (this.user && this.user.balance[0].amount == 0 && this.user.balance[1].amount == 0);
		},
		disable_rem_all() {
			return this.amount_0 || this.amount_1 || !this.user || !(this.user.balance[0].amount != 0 || this.user.balance[1].amount != 0);
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 30000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<div>
			<div v-if="!data && loading">
				<v-progress-linear indeterminate></v-progress-linear>
				<v-skeleton-loader type="table-row-divider@2"/>
			</div>
			
			<swap-user-info v-if="user_address" :address="address" :user="user_address" @user-update="arg => this.user = arg"></swap-user-info>
			
			<v-card v-if="data" class="my-2">
				<v-card-text>
					<v-row>
						<v-col>
							<v-select
								v-model="pool_idx"
								:items="fee_rates"
								:label="$t('swap.fee_level')"
								item-text="text"
								item-value="value"
							></v-select>
						</v-col>
						<v-col>
							<v-text-field class="text-align-right"
								v-model="price"
								:label="$t('common.price')"
								:suffix="data.symbols[1] + ' / ' + data.symbols[0]"
								disabled>
							</v-text-field>
						</v-col>
						<v-col>
							<v-text-field class="text-align-right"
								v-model="amount_0"
								:label="$t('swap.token_amount')"
								:suffix="data.symbols[0]">
							</v-text-field>
						</v-col>
						<v-col>
							<v-text-field class="text-align-right"
								v-model="amount_1"
								:label="$t('swap.currency_amount')"
								:suffix="data.symbols[1]">
							</v-text-field>
						</v-col>
					</v-row>
				</v-card-text>
				<v-card-actions class="justify-end">
					<v-btn @click="match()">{{ $t('swap.price_match') }}</v-btn>
					<v-btn @click="payout()" :disabled="disable_payout">{{ $t('swap.payout') }}</v-btn>
					<v-btn @click="switch_pool()" :disabled="disable_switch">{{ $t('swap.switch_fee') }}</v-btn>
					<v-btn color="green lighten-1" @click="submit(true)" :disabled="disable_add_rem || disable_add">{{ $t('swap.add_liquidity') }}</v-btn>
					<v-btn color="red lighten-1" @click="submit(false)" :disabled="disable_add_rem">{{ $t('swap.remove_iquidity') }}</v-btn>
					<v-btn color="red lighten-1" @click="rem_all_liquid()" :disabled="disable_rem_all">{{ $t('swap.remove_all') }}</v-btn>
				</v-card-actions>
			</v-card>
			
			<v-alert
				border="left"
				colored-border
				type="success"
				elevation="2"
				v-if="result"
				class="my-2"
			>
				{{ $t('common.transaction_has_been_sent') }}: <router-link :to="'/explore/transaction/' + result.id">{{result.id}}</router-link>
			</v-alert>

			<v-alert
				border="left"
				colored-border
				type="error"
				elevation="2"
				v-if="error"
				class="my-2"
			>
				{{ $t('common.failed_with') }}: <b>{{error}}</b>
			</v-alert>
		</div>
	`
})
