
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

Vue.component('swap-sub-menu', {
	props: {
		address: String
	},
	template: `
		<v-tabs>
			<v-tab :to="'/swap/trade/' + address">Trade</v-tab>
			<v-tab :to="'/swap/history/' + address">History</v-tab>
			<v-tab :to="'/swap/liquid/' + address">Liquidity</v-tab>
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
		</div>
	`
})

Vue.component('swap-user-info', {
	props: {
		address: String,
		user: String
	},
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
				});
		}
	},
	watch: {
		user() {
			update();
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
				<v-progress-linear indeterminate absolute top></v-progress-linear>
				<v-skeleton-loader type="table-row-divider@2"/>
			</div>

			<v-simple-table v-if="data">
				<thead>
					<tr>
						<th></th>
						<th>User Balance</th>
						<th>Symbol</th>
						<th>Contract</th>
						<th>Unlock</th>
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
						<td>{{data.unlock_height}}</td>
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
						<td>{{data.unlock_height}}</td>
					</tr>
				</tbody>
			</v-simple-table>
		</v-card>
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
			result: null,
			error: null,
		}
	},
	methods: {
		update() {
			fetch('/wapi/swap/info?id=' + this.address)
				.then(response => response.json())
				.then(data => this.data = data);
		},
		submit(index, amount) {
			const req = {};
			req.wallet = this.wallet;
			req.address = this.address;
			req.index = index;
			req.amount = parseFloat(amount);
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
						<v-card-actions>
							<v-btn color="green" @click="submit(1, buy_amount)">Buy</v-btn>
						</v-card-actions>
					</v-card>
				</v-col>
				<v-col>
					<v-card>
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
						<v-card-actions>
							<v-btn color="red" @click="submit(0, sell_amount)">Sell</v-btn>
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
			user_address: null,
			amount_0: null,
			amount_1: null,
			result: null,
			error: null,
		}
	},
	methods: {
		update() {
			fetch('/wapi/swap/info?id=' + this.address)
				.then(response => response.json())
				.then(data => this.data = data);
		},
		submit(mode) {
			const req = {};
			req.index = this.wallet;
			req.address = this.address;
			req.amount = [	parseFloat(amount_0) * Math.pow(10, data.decimals[0]),
							parseFloat(amount_1) * Math.pow(10, data.decimals[1])];
			fetch('/api/wallet/swap_' + (mode ? "add" : "rem") + "_liquid", {body: JSON.stringify(req), method: "post"})
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
		}
	},
	watch: {
		wallet(value) {
			fetch('/wapi/wallet/address?index=' + value)
				.then(response => response.json())
				.then(data => this.user_address = data[0]);
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
			<swap-user-info v-if="user_address" :address="address" :user="user_address"></swap-user-info>
			
			<v-card v-if="data" class="my-2">
				<v-card-text>
					<v-text-field class="text-align-right"
						v-model="amount_0"
						label="Token Amount"
						:suffix="data.symbols[0]">
					</v-text-field>
					<v-text-field class="text-align-right"
						v-model="amount_1"
						label="Currency Amount"
						:suffix="data.symbols[1]">
					</v-text-field>
				</v-card-text>
				<v-card-actions>
					<v-btn color="green" @click="submit(true)">Add Liquidity</v-btn>
					<v-btn color="red" @click="submit(false)">Remove Liquidity</v-btn>
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
