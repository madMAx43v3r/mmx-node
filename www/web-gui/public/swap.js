
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
				label="Token"
				item-text="text"
				item-value="value"
			></v-select>

			<v-select
				v-model="currency"
				:items="selectItems"
				label="Currency"
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
								<th>Name</th>
								<th>Token</th>
								<th>Currency</th>
								<th>Price</th>
								<th>Pool Balance</th>
								<th>Pool Balance</th>
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
								<td><b>{{ item.display_price ? parseFloat( (item.display_price).toPrecision(6) ) : "N/A" }}</b>&nbsp; {{item.symbols[1]}} / {{item.symbols[0]}}</td>
								<td><b>{{ parseFloat( (item.balance[0].value).toPrecision(6) ) }}</b>&nbsp; {{item.symbols[0]}}</td>
								<td><b>{{ parseFloat( (item.balance[1].value).toPrecision(6) ) }}</b>&nbsp; {{item.symbols[1]}}</td>
								<td>
									<v-btn :to="'/swap/trade/' + item.address" outlined text>Swap</v-btn>
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
			<v-tab :to="'/swap/trade/' + address">Trade</v-tab>
			<v-tab :to="'/swap/history/' + address">History</v-tab>
			<v-tab :to="'/swap/liquid/' + address">My Liquidity</v-tab>
			<v-tab :to="'/swap/pool/' + address">Pool State</v-tab>
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
				<v-chip label>Swap</v-chip>
				<v-chip label>{{data.display_price ? parseFloat((data.display_price).toPrecision(6)) : "N/A"}}&nbsp; {{data.symbols[1]}} / {{data.symbols[0]}}</v-chip>
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
							<th>Pool Balance</th>
							<th>Volume (24h)</th>
							<th>Volume (7 days)</th>
							<th>APY (24h)</th>
							<th>APY (7 days)</th>
						</tr>
					</thead>
					<tbody>
						<template v-for="i in [0, 1]">
							<tr>
								<td class="key-cell">{{i == 0 ? "Token" : "Currency"}}</td>
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
						<th>Fee Level</th>
						<th>Balance</th><th></th>
						<th>Balance</th><th></th>
						<th>User Total</th>
						<th>User Total</th>
					</tr>
				</thead>
				<tbody>
					<template v-for="(fee_rate, k) in data.fee_rates">
						<tr>
							<td class="key-cell">{{fee_rate * 100}} %</td>
							<template v-for="i in [0, 1]">
								<td><b>{{ parseFloat( (data.pools[k].balance[i].value).toPrecision(6) ) }}</b> {{data.symbols[i]}}</td>
								<td>{{(data.pools[k].balance[i].value / data.balance[i].value * 100).toFixed(1)}} %</td>
							</template>
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
						<th>My Balance</th>
						<th>My Liquidity</th>
						<th>Fees Earned</th>
						<th>Fee Level</th>
						<th>Unlock Height</th>
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
				{ text: "Height", value: 'height' },
				{ text: "Type", value: 'type' },
				{ text: "Amount", value: 'value' },
				{ text: "Symbol", value: 'symbol' },
				{ text: "User", value: 'user' },
				{ text: "Link", value: 'txid' },
				{ text: "Time", value: 'time' },
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
			buy_amount: null,
			buy_balance: null,
			buy_estimate: null,
			sell_fee: null,
			sell_amount: null,
			sell_balance: null,
			sell_estimate: null,
			result: null,
			error: null,
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
		submit(index, amount) {
			const req = {};
			req.wallet = this.wallet;
			req.address = this.address;
			req.index = index;
			req.amount = amount;
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
		}
	},
	watch: {
		wallet() {
			this.update_wallet();
		},
		buy_amount(value) {
			this.buy_estimate = null;
			if(value > 0) {
				fetch('/wapi/swap/trade_estimate?id=' + this.address + '&index=1&amount=' + value)
					.then(response => response.json())
					.then(data => {
						this.buy_fee = (100 * data.fee.value / (parseFloat(data.trade.value) + parseFloat(data.fee.value))).toFixed(2);
						this.buy_estimate = data.trade.value;
					});
			}
		},
		sell_amount(value) {
			this.sell_estimate = null;
			if(value > 0) {
				fetch('/wapi/swap/trade_estimate?id=' + this.address + '&index=0&amount=' + value)
					.then(response => response.json())
					.then(data => {
						this.sell_fee = (100 * data.fee.value / (parseFloat(data.trade.value) + parseFloat(data.fee.value))).toFixed(2);
						this.sell_estimate = data.trade.value;
					});
			}
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
										label="Wallet Balance"
										:suffix="data.symbols[1]" disabled>
									</v-text-field>
								</v-col>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="buy_amount"
										label="Buy Amount"
										:suffix="data.symbols[1]">
									</v-text-field>
								</v-col>
							</v-row>
							<v-row>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="buy_fee"
										label="Trade Fee (estimated)"
										suffix="%" disabled>
									</v-text-field>
								</v-col>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="buy_estimate"
										label="You receive (estimated)"
										:suffix="data.symbols[0]" disabled>
									</v-text-field>
								</v-col>
							</v-row>
						</v-card-text>
						<v-card-actions class="justify-end">
							<v-btn color="green lighten-1" @click="submit(1, buy_amount)" :disabled="!(buy_amount > 0)">Buy</v-btn>
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
										label="Wallet Balance"
										:suffix="data.symbols[0]" disabled>
									</v-text-field>
								</v-col>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="sell_amount"
										label="Sell Amount"
										:suffix="data.symbols[0]">
									</v-text-field>
								</v-col>
							</v-row>
							<v-row>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="sell_fee"
										label="Trade Fee (estimated)"
										suffix="%" disabled>
									</v-text-field>
								</v-col>
								<v-col>
									<v-text-field class="text-align-right"
										v-model="sell_estimate"
										label="You receive (estimated)"
										:suffix="data.symbols[1]" disabled>
									</v-text-field>
								</v-col>
							</v-row>
						</v-card-text>
						<v-card-actions class="justify-end">
							<v-btn color="red lighten-1" @click="submit(0, sell_amount)" :disabled="!(sell_amount > 0)">Sell</v-btn>
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
				this.amount_1 = parseFloat((this.amount_0 * this.data.display_price).toFixed(this.data.decimals[1]));
			} else if(!this.amount_0 && this.amount_1) {
				this.amount_0 = parseFloat((this.amount_1 / this.data.display_price).toFixed(this.data.decimals[0]));
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
								label="Fee Level"
								item-text="text"
								item-value="value"
							></v-select>
						</v-col>
						<v-col>
							<v-text-field class="text-align-right"
								v-model="price"
								label="Price"
								:suffix="data.symbols[1] + ' / ' + data.symbols[0]"
								disabled>
							</v-text-field>
						</v-col>
						<v-col>
							<v-text-field class="text-align-right"
								v-model="amount_0"
								label="Token Amount"
								:suffix="data.symbols[0]">
							</v-text-field>
						</v-col>
						<v-col>
							<v-text-field class="text-align-right"
								v-model="amount_1"
								label="Currency Amount"
								:suffix="data.symbols[1]">
							</v-text-field>
						</v-col>
					</v-row>
				</v-card-text>
				<v-card-actions class="justify-end">
					<v-btn @click="match()">Price Match</v-btn>
					<v-btn @click="payout()" :disabled="disable_payout">Payout</v-btn>
					<v-btn @click="switch_pool()" :disabled="disable_switch">Switch Fee</v-btn>
					<v-btn color="green lighten-1" @click="submit(true)" :disabled="disable_add_rem || disable_add">Add Liquidity</v-btn>
					<v-btn color="red lighten-1" @click="submit(false)" :disabled="disable_add_rem">Remove Liquidity</v-btn>
					<v-btn color="red lighten-1" @click="rem_all_liquid()" :disabled="disable_rem_all">Remove All</v-btn>
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
