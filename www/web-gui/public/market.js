
Vue.component('market-menu', {
	props: {
		wallet_: Number,
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
			fetch('/wapi/wallet/accounts')
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.wallets = data;
					if(this.wallet == null && data.length) {
						this.wallet = data[0].account;
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
			localStorage.setItem('active_wallet', this.wallet);
			this.$router.push('/market/' + page + '/' + this.wallet + '/' + this.bid + '/' + this.ask).catch(()=>{});
		}
	},
	created() {
		if(this.wallet_ != null) {
			this.wallet = this.wallet_;
		} else {
			this.wallet = get_active_wallet();
		}
		this.update();
	},
	watch: {
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
		<div class="mb-2">
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
							:label="$t('common.wallet')"
							item-text="account"
							item-value="account">
							<template v-for="slotName in ['item', 'selection']" v-slot:[slotName]="{ item }">
								{{ $t('market_menu.wallet') }} #{{item.account}} ({{item.address}})
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
					<v-tab :to="'/market/offers/' + wallet + '/' + bid + '/' + ask">{{ $t('market_menu.offers') }}</v-tab>
					<v-tab :to="'/market/history/' + wallet + '/' + bid + '/' + ask">{{ $t('market_menu.history') }}</v-tab>
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
	data() {
		return {
			data: null,
			offer: {},
			tokens: null,
			bid_symbol: null,
			bid_decimals: null,
			min_bid_amount: null,
			accepted: new Set(),
			timer: null,
			result: null,
			error: null,
			loading: false,
			trade_dialog: false,
			accept_dialog: false,
			trade_amount: null,
			trade_estimate: null,
			trade_estimate_data: null,
			wallet_balance: null,
			fee_ratio: 1024,
			fee_amount: null,
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
				if(this.min_bid_amount > 0) {
					query += '&min_bid=' + parseInt(this.min_bid_amount * Math.pow(10, this.bid_decimals));
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
							if(token.currency == this.bid) {
								this.bid_symbol = token.symbol;
								this.bid_decimals = token.decimals;
							}
							this.tokens.add(token.currency);
						}
						this.update();
					});
			}
		},
		update_estimate() {
			this.trade_estimate = null;
			fetch('/wapi/offer/trade_estimate?id=' + this.offer.address + '&amount=' + (this.trade_amount > 0 ? this.trade_amount : 0))
				.then(response => response.json())
				.then(data => {
					this.trade_estimate = data.trade.value;
					this.trade_estimate_data = data;
				});
			this.submit_trade(true);
		},
		update_fee_ratio(value) {
			this.fee_ratio = value;
			if(this.trade_dialog) {
				this.submit_trade(true);
			}
			if(this.accept_dialog) {
				this.submit_accept(true);
			}
		},
		update_balance(currency) {
			this.wallet_balance = null;
			fetch('/wapi/wallet/balance?index=' + this.wallet + '&currency=' + currency)
				.then(response => response.json())
				.then(data => this.wallet_balance = data ? data.spendable : 0);
		},
		trade(offer) {
			this.update_balance(offer.ask_currency);
			this.offer = offer;
			this.fee_amount = null;
			this.trade_amount = null;
			this.trade_estimate = null;
			this.trade_dialog = true;
		},
		accept(offer) {
			this.update_balance(offer.ask_currency);
			this.offer = offer;
			this.fee_amount = null;
			this.submit_accept(true);
			this.accept_dialog = true;
		},
		increase() {
			this.trade_amount = this.trade_estimate_data.next_input.value;
		},
		submit_trade(estimate = false) {
			const req = {};
			req.index = this.wallet;
			req.address = this.offer.address;
			req.amount = this.trade_amount;
			req.options = {};
			req.options.fee_ratio = this.fee_ratio;
			if(estimate) {
				req.options.auto_send = false;
			}
			if(!(req.amount > 0)) {
				return;
			}
			fetch('/wapi/wallet/offer_trade', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						response.json().then(data => {
							this.error = null;
							if(estimate) {
								this.fee_amount = data.exec_result.total_fee_value;
							} else {
								this.result = data;
							}
						});
					} else if(!estimate) {
						response.text().then(data => {
							this.error = data;
							this.result = null;
						});
					}
				});
			if(!estimate) {
				this.trade_dialog = false;
			}
		},
		submit_accept(estimate = false) {
			const address = this.offer.address;
			const req = {};
			req.index = this.wallet;
			req.address = address;
			req.options = {};
			req.options.fee_ratio = this.fee_ratio;
			if(estimate) {
				req.options.auto_send = false;
			}
			fetch('/wapi/wallet/accept_offer', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						response.json().then(data => {
							this.error = null;
							if(estimate) {
								this.fee_amount = data.exec_result.total_fee_value;
							} else {
								this.result = data;
								this.accepted.add(address);
							}
						});
					} else if(!estimate) {
						response.text().then(data => {
							this.error = data;
							this.result = null;
						});
					}
				});
			if(!estimate) {
				this.accept_dialog = false;
			}
		},
		is_valid_trade() {
			return this.offer
				&& this.trade_amount > 0
				&& this.trade_amount <= this.wallet_balance
				&& this.trade_estimate_data
				&& this.trade_estimate_data.trade.amount > 0
				&& this.trade_estimate_data.trade.amount <= this.offer.bid_balance;
		},
		is_valid_trade_amount() {
			if(this.trade_amount > this.wallet_balance) {
				return "insufficient funds";
			}
			if(this.trade_amount == null || this.trade_amount == "" || this.trade_amount > 0) {
				return true;
			}
			return "invalid amount";
		},
		is_valid_bid_amount() {
			if(this.trade_estimate != null) {
				if(this.trade_estimate > this.offer.bid_balance_value) {
					return "exceeding offer";
				}
			}
			return true;
		}
	},
	watch: {
		trade_amount() {
			if(this.offer) {
				this.update_estimate();
			}
		},
		min_bid_amount() {
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
			<v-alert border="left" colored-border type="success" elevation="2" v-if="result" class="my-2">
				{{ $t('common.transaction_has_been_sent') }}: <router-link :to="'/explore/transaction/' + result.id">{{result.id}}</router-link>
			</v-alert>
			<v-alert border="left" colored-border type="error" elevation="2" v-if="error" class="my-2">
				{{ $t('common.failed_with') }}: <b>{{error}}</b>
			</v-alert>

			<v-dialog v-model="trade_dialog" max-width="900">
				<template v-slot:default="dialog">
					<v-card>
						<v-toolbar color="primary" dark></v-toolbar>
						<v-card-title>
							Trade - {{offer.address}}
						</v-card-title>
						<v-card-text>
							<v-row>
								<v-col class="pb-0">
									<v-text-field class="text-align-right"
										v-model="wallet_balance"
										:label="$t('market_offers.wallet_ballance')"
										:suffix="offer.ask_symbol" disabled>
									</v-text-field>
								</v-col>
								<v-col class="pb-0">
									<v-text-field class="text-align-right"
										v-model="trade_amount"
										:label="$t('market_offers.you_send')"
										:suffix="offer.ask_symbol"
										:rules="[is_valid_trade_amount]">
									</v-text-field>
								</v-col>
							</v-row>
							<v-row>
								<v-col class="py-0">
									<v-text-field class="text-align-right"
										v-model="offer.bid_balance_value"
										label="They Offer"
										:suffix="offer.bid_symbol" disabled>
									</v-text-field>
								</v-col>
								<v-col class="py-0">
									<v-text-field class="text-align-right"
										v-model="trade_estimate"
										:label="$t('market_offers.you_receive')"
										:suffix="offer.bid_symbol"
										:rules="[is_valid_bid_amount]" disabled>
									</v-text-field>
								</v-col>
							</v-row>
							<v-row justify="end">
								<v-col cols="3">
									<tx-fee-select @update-value="value => update_fee_ratio(value)"></tx-fee-select>
								</v-col>
								<v-col cols="3">
									<v-text-field class="text-align-right"
										label="TX Fee"
										v-model.number="fee_amount" suffix="MMX" disabled>
									</v-text-field>
								</v-col>
							</v-row>
						</v-card-text>
						<v-card-actions class="justify-end">
							<v-btn color="primary" @click="submit_trade()" :disabled="!is_valid_trade()">{{ $t('market_offers.trade') }}</v-btn>
							<v-btn @click="increase()">{{ $t('market_offers.increase') }}</v-btn>
							<v-btn @click="trade_dialog = false">{{ $t('market_offers.cancel') }}</v-btn>
						</v-card-actions>
					</v-card>
				</template>
			</v-dialog>
			
			<v-dialog v-model="accept_dialog" max-width="900">
				<template v-slot:default="dialog">
					<v-card>
						<v-toolbar color="primary" dark></v-toolbar>
						<v-card-title>
							Accept - {{offer.address}}
						</v-card-title>
						<v-card-text>
							<v-row>
								<v-col class="pb-0">
									<v-text-field class="text-align-right"
										v-model="wallet_balance"
										:label="$t('market_offers.wallet_ballance')"
										:suffix="offer.ask_symbol" disabled>
									</v-text-field>
								</v-col>
								<v-col class="pb-0">
									<v-text-field class="text-align-right"
										v-model="offer.ask_value"
										:label="$t('market_offers.you_send')"
										:suffix="offer.ask_symbol" disabled>
									</v-text-field>
								</v-col>
							</v-row>
							<v-row justify="end">
								<v-col cols="6" class="py-0">
									<v-text-field class="text-align-right"
										v-model="offer.bid_balance_value"
										:label="$t('market_offers.you_receive')"
										:suffix="offer.bid_symbol" disabled>
									</v-text-field>
								</v-col>
							</v-row>
							<v-row justify="end">
								<v-col cols="3">
									<tx-fee-select @update-value="value => update_fee_ratio(value)"></tx-fee-select>
								</v-col>
								<v-col cols="3">
									<v-text-field class="text-align-right"
										label="TX Fee"
										v-model.number="fee_amount" suffix="MMX" disabled>
									</v-text-field>
								</v-col>
							</v-row>
						</v-card-text>
						<v-card-actions class="justify-end">
							<v-btn color="primary" @click="submit_accept()">{{ $t('market_offers.accept') }}</v-btn>
							<v-btn @click="accept_dialog = false">{{ $t('market_offers.cancel') }}</v-btn>
						</v-card-actions>
					</v-card>
				</template>
			</v-dialog>
			
			<template v-if="bid">
				<v-card class="my-2">
					<v-card-text>
						<v-row>
							<v-col cols="3" class="pb-0">
								<v-text-field class="text-align-right"
									v-model="min_bid_amount"
									label="Minimum Offer"
									:suffix="bid_symbol">
								</v-text-field>
							</v-col>
						</v-row>
					</v-card-text>
				</v-card>
			</template>

			<v-card>
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
								<th></th>
								<th></th>
							</tr>
						</thead>
						<tbody>
							<tr v-for="item in data" :key="item.address">
								<td>
									<b>{{item.bid_balance_value}}</b>&nbsp;
									<template v-if="item.bid_symbol == 'MMX'">MMX</template>
									<template v-else>
										<router-link :to="'/explore/address/' + item.bid_currency">
											<template v-if="tokens.has(item.bid_currency)">{{item.bid_symbol}}</template>
											<template v-else>{{item.bid_symbol}}?</template>
										</router-link>
									</template>
								</td>
								<td>
									<b>{{parseFloat((item.bid_balance_value * item.display_price).toPrecision(6))}}</b>&nbsp;
									<template v-if="item.ask_symbol == 'MMX'">MMX</template>
									<template v-else>
										<router-link :to="'/explore/address/' + item.ask_currency">
											<template v-if="tokens.has(item.ask_currency)">{{item.ask_symbol}}</template>
											<template v-else>{{item.ask_symbol}}?</template>
										</router-link>
									</template>
								</td>
								<td><b>{{ parseFloat( (item.display_price).toPrecision(3) ) }}</b>&nbsp; {{item.ask_symbol}} / {{item.bid_symbol}}</td>
								<td><b>{{ parseFloat( (1 / item.display_price).toPrecision(3) ) }}</b>&nbsp; {{item.bid_symbol}} / {{item.ask_symbol}}</td>
								<td>{{new Date(item.time * 1000).toLocaleString()}}</td>
								<td><router-link :to="'/explore/address/' + item.address">TX</router-link></td>
								<td>
									<v-btn outlined text @click="trade(item)">{{ $t('market_offers.trade') }}</v-btn>
									<template v-if="!accepted.has(item.address)">
										<v-btn outlined text color="green darken-1" @click="accept(item)">{{ $t('common.accept') }}</v-btn>
									</template>
								</td>
							</tr>
						</tbody>
					</v-simple-table>
				</template>
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
								<th></th>
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
								<td><b>{{parseFloat((item.display_price).toPrecision(3))}}</b>&nbsp; {{item.ask_symbol}} / {{item.bid_symbol}}</td>
								<td><b>{{parseFloat((1 / item.display_price).toPrecision(3))}}</b>&nbsp; {{item.bid_symbol}} / {{item.ask_symbol}}</td>
								<td>{{new Date(item.time * 1000).toLocaleString()}}</td>
								<td><router-link :to="'/explore/address/' + item.address">Offer</router-link></td>
								<td><router-link :to="'/explore/transaction/' + item.txid">TX</router-link></td>
							</tr>
						</tbody>
					</v-simple-table>
				</template>
			</v-card>
		</div>
	`
})
