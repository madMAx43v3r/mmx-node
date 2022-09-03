
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
		wallet(value) {
			this.submit();
		},
		bid(value) {
			this.submit();
		},
		ask(value) {
			this.submit();
		}
	},
	computed: {
		selectItems() {
			var result = [];
			
			result.push({ text: this.$t('market_menu.anything'), value: null});

			this.tokens.map(item => {
				var text = item.symbol;
				if(item.currency != 'mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev') {
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

				<v-btn-toggle
					v-model="currentItem"
					dense
					class="my-2"
				>
					<v-btn @click="submit('offers')">{{ $t('market_menu.offers') }}</v-btn>
				</v-btn-toggle>
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
			tokens: new Set(),
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
			fetch('/wapi/wallet/tokens')
				.then(response => response.json())
				.then(data => {
					this.tokens.clear();
					for(const token of data) {
						this.tokens.add(token.currency);
					}
				});
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
			<v-alert
				border="left"
				colored-border
				type="success"
				elevation="2"
				v-if="result"
				class="my-2"
			>
				{{ $t('common.transaction_has_been_sent') }}: <router-link :to="'/explore/transaction/' + result">{{result}}</router-link>
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

			<v-dialog v-model="dialog"
				transition="dialog-top-transition"
				max-width="800"
			>
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
										<template v-for="entry in offer.bids">
											<td>{{ $t('market_offers.you_receive') }}</td>
											<td>
												<b>{{entry.value}}</b> {{entry.symbol}} 
												<template v-if="!entry.is_native">
													- [<router-link :to="'/explore/address/' + entry.contract">{{entry.contract}}</router-link>]
												</template>
											</td>
										</template>
									</tr>
									<tr>
										<template v-for="entry in offer.asks">
											<td>{{ $t('market_offers.you_pay') }}</td>
											<td>
												<b>{{entry.value}}</b> {{entry.symbol}} 
												<template v-if="!entry.is_native">
													- [<router-link :to="'/explore/address/' + entry.contract">{{entry.contract}}</router-link>]
												</template>
											</td>
										</template>
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
								<th>{{ $t('market_offers.they_offer') }}</th>
								<th>{{ $t('market_offers.they_ask') }}</th>
								<template v-if="bid && ask">
									<th>{{ $t('market_offers.price') }}</th>
								</template>
								<th>{{ $t('market_offers.time') }}</th>
								<th>{{ $t('market_offers.link') }}</th>
								<th></th>
							</thead>
							<tbody>
								<tr v-for="item in data" :key="item.address" :class="{positive: accepted.has(item.address)}">
									<td>
										<template v-for="(entry, index) in item.bids">
											<template v-if="index">, </template><b>{{entry.value}}</b>&nbsp;
											<template v-if="entry.is_native">{{entry.symbol}}</template>
											<template v-else>
												<router-link :to="'/explore/address/' + entry.contract">
													<template v-if="tokens.has(entry.contract)">{{entry.symbol}}</template>
													<template v-else>{{entry.symbol}}?</template>
												</router-link>
											</template>
										</template>
									</td>
									<td>
										<template v-for="(entry, index) in item.asks">
											<template v-if="index">, </template><b>{{entry.value}}</b>&nbsp;
											<template v-if="entry.is_native">{{entry.symbol}}</template>
											<template v-else>
												<router-link :to="'/explore/address/' + entry.contract">
													<template v-if="tokens.has(entry.contract)">{{entry.symbol}}</template>
													<template v-else>{{entry.symbol}}?</template>
												</router-link>
											</template>
										</template>
									</td>
									<template v-if="bid && ask">
										<td><b>{{item.price}}</b></td>
									</template>
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
