
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
			this.$router.push('/swap_market/' + this.token + '/' + this.currency).catch(()=>{});
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
		<div class="my-2">
			<div v-if="loading">
				<v-card>
					<v-card-text>
						<v-progress-linear indeterminate absolute top></v-progress-linear>
						<v-skeleton-loader type="heading" class="my-6"/>
						<v-skeleton-loader type="heading" class="my-6"/>
					</v-card-text>
				</v-card>
			</div>

			<div v-if="!loading">
				<v-card>
					<v-card-text>
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
					</v-card-text>
				</v-card>
			</div>
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
								<td><router-link :to="'/swap/trade/' + item.address + '/null'">
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
