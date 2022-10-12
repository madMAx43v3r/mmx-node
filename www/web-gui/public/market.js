
Vue.component('market-menu', {
	props: {
		wallet_: Number,
		bid_: null,
		ask_: null,
		page: String, 
		loading: false
	},
	data() {
		return {
			wallets: [],
			wallet: null,
			tokens: [],
			bid: null,
			ask: null,
			currentItem: 0
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/api/wallet/get_all_accounts')
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.wallets = data;
					if(!this.wallet && this.wallet_) {
						this.wallet = this.wallet_;
					}
					else if(data.length > 0) {
						this.wallet = data[0][0];
					}
				})
				.catch(error => {
					setTimeout(this.update, 1000);
					throw(error);
				});
			fetch('/wapi/wallet/tokens')
				.then(response => response.json())
				.then(data => this.tokens = data);
		},
		submit(page) {
			if(!page) {
				if(this.page) {
					page = this.page;
				} else {
					page = "offers";
				}
			}
			this.$router.push('/market/' + page + '/' + this.wallet + '/' + this.bid + '/' + this.ask).catch(()=>{});
		}
	},
	created() {
		this.update();
	},
	watch: {
		$route (to, from){				
			if(to.path == '/market') {
				this.submit();
			}
		},
		wallet() {
			this.submit();
		},
		bid() {
			this.submit();
		},
		ask() {
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
						<v-skeleton-loader type="heading" class="my-6"/>
					</v-card-text>
				</v-card>

				<v-skeleton-loader type="button" class="my-6"/>
			</div>

			<div v-if="!loading">
				<v-card>
					<v-card-text>					

						<v-select
							v-model="wallet"
							:items="wallets"
							:lablel="$t('market_menu.wallet')"						
							item-text="[0]"
							item-value="[0]">
							<template v-for="slotName in ['item', 'selection']" v-slot:[slotName]="{ item }">
								{{ $t('market_menu.wallet') }} #{{item[0]}}
							</template>				
						</v-select>

						<v-select
							v-model="bid"
							:items="selectItems"
							:label="$t('market_menu.they_offer')"
							item-text="text"
							item-value="value"
						></v-select>

						<v-select
							v-model="ask"
							:items="selectItems"
							:label="$t('market_menu.they_ask')"
							item-text="text"
							item-value="value"			
						></v-select>

					</v-card-text>
				</v-card>

				<v-tabs>
					<v-tab :to="'/market/offers/' + this.wallet + '/' + this.bid + '/' + this.ask">{{ $t('market_menu.offers') }}</v-tab>
					<v-tab :to="'/market/history/' + this.wallet + '/' + this.bid + '/' + this.ask">History</v-tab>
				</v-tabs>
			</div>

		</div>

		`
})

Vue.component('market-offers', {
	props: {
		wallet: Number,
		bid: null,
		ask: null,
		limit: Number
	},
	emits: [
		"accept-offer"
	],
	data() {
		return {
			data: null,
			offer: {},
			tokens: null,
			accepted: new Set(),
			timer: null,
			result: null,
			error: null,
			loading: false,
			dialog: false
		}
	},
	methods: {
		update() {
			if(this.tokens) {
				this.loading = true;
				let query = '/wapi/node/offers?limit=' + this.limit;
				if(this.bid) {
					query += '&bid=' + this.bid;
				}
				if(this.ask) {
					query += '&ask=' + this.ask;
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
		},
		confirm(offer) {
			this.offer = offer;
			this.dialog = true;
		},
		accept(offer) {
			const req = {};
			req.index = this.wallet;
			req.address = offer.address;
			fetch('/wapi/wallet/accept', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						response.json().then(data => {
							this.error = null;
							this.result = data;
							this.accepted.add(offer.address);
							this.$emit('accept-offer', {offer: offer, result: data});							
						});
					} else {
						response.text().then(data => {
							this.error = data;
							this.result = null;
						});
					}
					this.dialog = false;
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
		<div>
			<v-alert border="left" colored-border type="success" elevation="2" v-if="result" class="my-2">
				{{ $t('common.transaction_has_been_sent') }}: <router-link :to="'/explore/transaction/' + result.id">{{result.id}}</router-link>
			</v-alert>
			<v-alert border="left" colored-border type="error" elevation="2" v-if="error" class="my-2">
				{{ $t('common.failed_with') }}: <b>{{error}}</b>
			</v-alert>

			<v-dialog v-model="dialog" max-width="800">
				<template v-slot:default="dialog">
					<v-card>
						<v-toolbar color="primary" dark>
							{{ $t('market_offers.accept_offer') }}
						</v-toolbar>
						<v-card-text>
							<v-container>
								<v-simple-table>
									<tbody>
									<tr>
										<td>{{ $t('common.address') }}</td>
										<td><router-link :to="'/explore/address/' + offer.address">{{offer.address}}</router-link></td>
									</tr>
									<tr>
										<td>{{ $t('market_offers.you_receive') }}</td>
										<td>
											<b>{{offer.bid_value}}</b> {{offer.bid_symbol}}
											<template v-if="offer.bid_symbol != 'MMX'">
												- [<router-link :to="'/explore/address/' + offer.bid_currency">{{offer.bid_currency}}</router-link>]
											</template>
										</td>
									</tr>
									<tr>
										<td>{{ $t('market_offers.you_pay') }}</td>
										<td>
											<b>{{offer.ask_value}}</b> {{offer.ask_symbol}}
											<template v-if="offer.ask_symbol != 'MMX'">
												- [<router-link :to="'/explore/address/' + offer.ask_currency">{{offer.ask_currency}}</router-link>]
											</template>
										</td>
									</tr>
									</tbody>					
								</v-simple-table>
							</v-container>
						</v-card-text>
						<v-card-actions class="justify-end">
							<v-btn text color="primary" @click="accept(offer)">{{ $t('market_offers.accept') }}</v-btn>
							<v-btn text @click="dialog.value = false">{{ $t('market_offers.cancel') }}</v-btn>
						</v-card-actions>
					</v-card>
				</template>
			</v-dialog>		

			<v-card>
				<v-card-text>

					<div v-if="!data && loading">
						<v-progress-linear indeterminate absolute top></v-progress-linear>
						<v-skeleton-loader type="table-row-divider@6"/>
					</div>

					<template v-if="data">
						<v-simple-table>
							<thead>
								<tr>
									<th>{{ $t('market_offers.they_offer') }}</th>
									<th>{{ $t('market_offers.they_ask') }}</th>
									<th>{{ $t('market_offers.price') }}</th>
									<th>{{ $t('market_offers.price') }}</th>
									<th>{{ $t('market_offers.time') }}</th>
									<th>{{ $t('market_offers.link') }}</th>
									<th></th>
								</tr>
							</thead>
							<tbody>
								<tr v-for="item in data" :key="item.address" :class="{positive: accepted.has(item.address)}">
									<td>
										<b>{{item.bid_value}}</b>&nbsp;
										<template v-if="item.bid_symbol == 'MMX'">MMX</template>
										<template v-else>
											<router-link :to="'/explore/address/' + item.bid_currency">
												<template v-if="tokens.has(item.bid_currency)">{{item.bid_symbol}}</template>
												<template v-else>{{item.bid_symbol}}?</template>
											</router-link>
										</template>
									</td>
									<td>
										<b>{{item.ask_value}}</b>&nbsp;
										<template v-if="item.ask_symbol == 'MMX'">MMX</template>
										<template v-else>
											<router-link :to="'/explore/address/' + item.ask_currency">
												<template v-if="tokens.has(item.ask_currency)">{{item.ask_symbol}}</template>
												<template v-else>{{item.ask_symbol}}?</template>
											</router-link>
										</template>
									</td>
									<td><b>{{ parseFloat( (item.price).toPrecision(3) ) }}</b>&nbsp; {{item.ask_symbol}} / {{item.bid_symbol}}</td>
									<td><b>{{ parseFloat( (1 / item.price).toPrecision(3) ) }}</b>&nbsp; {{item.bid_symbol}} / {{item.ask_symbol}}</td>
									<td>{{new Date(item.time * 1000).toLocaleString()}}</td>
									<td><router-link :to="'/explore/address/' + item.address">{{ $t('market_offers.address') }}</router-link></td>
									<td>
										<template v-if="!accepted.has(item.address)">
											<v-btn outlined text @click="confirm(item)">{{ $t('market_offers.accept') }}</v-btn>
										</template>
									</td>
								</tr>
							</tbody>
						</v-simple-table>
					</template>
				</v-card-text>
			</v-card>
		</div>
		`
})

Vue.component('market-history', {
	props: {
		bid: null,
		ask: null,
		limit: Number
	},
	data() {
		return {
			data: null,
			tokens: null,
			timer: null,
			loading: false
		}
	},
	methods: {
		update() {
			if(this.tokens) {
				this.loading = true;
				let query = '/wapi/node/trade_history?limit=' + this.limit;
				if(this.bid) {
					query += '&bid=' + this.bid;
				}
				if(this.ask) {
					query += '&ask=' + this.ask;
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
				<v-card-text>
					<div v-if="!data && loading">
						<v-progress-linear indeterminate absolute top></v-progress-linear>
						<v-skeleton-loader type="table-row-divider@6"/>
					</div>

					<template v-if="data">
						<v-simple-table>
							<thead>
								<tr>
									<th>{{ $t('market_offers.they_offer') }}</th>
									<th>{{ $t('market_offers.they_ask') }}</th>
									<th>{{ $t('market_offers.price') }}</th>
									<th>{{ $t('market_offers.price') }}</th>
									<th>{{ $t('market_offers.time') }}</th>
									<th>{{ $t('market_offers.link') }}</th>
									<th></th>
								</tr>
							</thead>
							<tbody>
								<tr v-for="item in data" :key="item.address">
									<td>
										<b>{{item.bid_value}}</b>&nbsp;
										<template v-if="item.bid_symbol == 'MMX'">MMX</template>
										<template v-else>
											<router-link :to="'/explore/address/' + item.bid_currency">
												<template v-if="tokens.has(item.bid_currency)">{{item.bid_symbol}}</template>
												<template v-else>{{item.bid_symbol}}?</template>
											</router-link>
										</template>
									</td>
									<td>
										<b>{{item.ask_value}}</b>&nbsp;
										<template v-if="item.ask_symbol == 'MMX'">MMX</template>
										<template v-else>
											<router-link :to="'/explore/address/' + item.ask_currency">
												<template v-if="tokens.has(item.ask_currency)">{{item.ask_symbol}}</template>
												<template v-else>{{item.ask_symbol}}?</template>
											</router-link>
										</template>
									</td>
									<td><b>{{parseFloat((item.price).toPrecision(3))}}</b>&nbsp; {{item.ask_symbol}} / {{item.bid_symbol}}</td>
									<td><b>{{parseFloat((1 / item.price).toPrecision(3))}}</b>&nbsp; {{item.bid_symbol}} / {{item.ask_symbol}}</td>
									<td>{{new Date(item.close_time * 1000).toLocaleString()}}</td>
									<td><router-link :to="'/explore/address/' + item.address">{{ $t('market_offers.address') }}</router-link></td>
									<td><router-link :to="'/explore/transaction/' + item.trade_txid">TX</router-link></td>
								</tr>
							</tbody>
						</v-simple-table>
					</template>
				</v-card-text>
			</v-card>
		</div>
	`
})
