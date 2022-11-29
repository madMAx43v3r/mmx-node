
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
								<td><b>{{ parseFloat( (item.price).toPrecision(6) ) }}</b>&nbsp; {{item.symbols[1]}} / {{item.symbols[0]}}</td>
								<td><b>{{ parseFloat( (item.balance[0].value).toPrecision(6) ) }}</b>&nbsp; {{item.symbols[0]}}</td>
								<td><b>{{ parseFloat( (item.balance[1].value).toPrecision(6) ) }}</b>&nbsp; {{item.symbols[1]}}</td>
								<td><router-link :to="'/swap/trade/' + item.address">
									<v-btn outlined text>Swap</v-btn>
								</router-link></td>
							</tr>
						</tbody>
					</v-simple-table>
				</template>
			</v-card>
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
			buy_amount: null,
			buy_estimate: null,
			sell_amount: null,
			sell_estimate: null,
			timer: null,
			loading: false,
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
		},
		buy_submit() {
			// TODO
		}
	},
	watch: {
		buy_amount(value) {
			this.buy_estimate = null;
			fetch('/wapi/swap/trade_estimate?id=' + this.address + '&index=1' + '&amount=' + value)
				.then(response => response.json())
				.then(data => {
					this.buy_estimate = data.trade_amount;
				});
		},
		sell_amount(value) {
			this.sell_estimate = null;
			fetch('/wapi/swap/trade_estimate?id=' + this.address + '&index=0' + '&amount=' + value)
				.then(response => response.json())
				.then(data => {
					this.sell_estimate = data.trade_amount;
				});
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
		<div class="my-2">
			<template v-if="data">
				<v-chip label>Swap</v-chip>
				<v-chip label>{{parseFloat((data.price).toPrecision(6))}}&nbsp; {{data.symbols[1]}} / {{data.symbols[0]}}</v-chip>
				<v-chip label>{{data.address}}</v-chip>
			</template>
			
			<v-card class="my-2">
				<div v-if="!data && loading">
					<v-progress-linear indeterminate absolute top></v-progress-linear>
					<v-skeleton-loader type="table-row-divider@2"/>
				</div>

				<v-simple-table v-if="data">
					<thead>
						<tr>
							<th></th>
							<th>Pool Balance</th>
							<th>Symbol</th>
							<th>Contract</th>
						</tr>
					</thead>
					<tbody>
						<tr>
							<td class="key-cell">Token</td>
							<td><b>{{ parseFloat( (data.balance[0].value).toPrecision(6) ) }}</b></td>
							<td>{{data.symbols[0]}}</td>
							<td>
								<template v-if="data.symbols[0] != 'MMX'">
									<router-link :to="'/explore/address/' + data.tokens[0]">{{data.tokens[0]}}</router-link>
								</template>
							</td>
						</tr>
						<tr>
							<td class="key-cell">Currency</td>
							<td><b>{{ parseFloat( (data.balance[1].value).toPrecision(6) ) }}</b></td>
							<td>{{data.symbols[1]}}</td>
							<td>
								<template v-if="data.symbols[1] != 'MMX'">
									<router-link :to="'/explore/address/' + data.tokens[1]">{{data.tokens[1]}}</router-link>
								</template>
							</td>
						</tr>
					</tbody>
				</v-simple-table>
			</v-card>
			
			<v-container v-if="data">
				<v-row>
					<v-col>
						<v-card class="my-2">
							<v-card-text>
								<v-text-field class="text-align-right"
									v-model="buy_amount"
									label="Buy Amount"
									:suffix="data.symbols[1]">
								</v-text-field>
								<v-text-field class="text-align-right"
									v-model="buy_estimate"
									label="You receive (estimated)"
									:suffix="data.symbols[0]" disabled>
								</v-text-field>
							</v-card-text>
						</v-card>
					</v-col>
					<v-col>
						<v-card class="my-2">
							<v-card-text>
								<v-text-field class="text-align-right"
									v-model="sell_amount"
									label="Sell Amount"
									:suffix="data.symbols[0]">
								</v-text-field>
								<v-text-field class="text-align-right"
									v-model="sell_estimate"
									label="You receive (estimated)"
									:suffix="data.symbols[1]" disabled>
								</v-text-field>
							</v-card-text>
						</v-card>
					</v-col>
				</v-row>
			</v-container>
		</div>
	`
})
