
app.component('market-menu', {
	props: {
		wallet_: Number,
		bid_: null,
		ask_: null,
		page: String
	},
	data() {
		return {
			wallets: [],
			wallet: null,
			tokens: [],
			bid: null,
			ask: null,
		}
	},
	methods: {
		update() {
			fetch('/api/wallet/get_all_accounts')
				.then(response => response.json())
				.then(data => {
					this.wallets = data;
					if(!this.wallet && this.wallet_) {
						this.wallet = this.wallet_;
					}
					else if(data.length > 0) {
						this.wallet = data[0][0];
					}
				});
		},
		submit(page) {
			if(!page) {
				if(this.page) {
					page = this.page;
				} else {
					page = "offers";
				}
			}
			this.$router.push('/market/' + page + '/' + this.wallet + '/' + this.bid + '/' + this.ask);
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
	template: `
		<div class="ui form segment">
			<div class="field">
				<label>Wallet</label>
				<select v-model="wallet">
					<option v-for="item in wallets" :key="item[0]" :value="item[0]">Wallet #{{item[0]}}</option>
				</select>
			</div>
			<div class="field">
				<label>They Offer</label>
				<select v-model="bid">
					<option :value="null">Anything</option>
					<option v-for="item in tokens" :key="item.currency" :value="item.currency">
						{{item.symbol}}<template v-if="item.currency != 'mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev'"> - [{{item.currency}}]</template>
					</option>
				</select>
			</div>
			<div class="field">
				<label>They Ask</label>
				<select v-model="ask">
					<option :value="null">Anything</option>
					<option v-for="item in tokens" :key="item.currency" :value="item.currency">
						{{item.symbol}}<template v-if="item.currency != 'mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev'"> - [{{item.currency}}]</template>
					</option>
				</select>
			</div>
		</div>
		<div class="ui large pointing menu">
			<a class="item" :class="{active: $route.meta.page == 'offers'}" @click="submit('offers')">Offers</a>
		</div>
		`
})

app.component('market-offers', {
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
			data: [],
			offer: {},
			timer: null,
			result: null,
			error: null
		}
	},
	methods: {
		update() {
			let query = '/wapi/node/offers?limit=' + this.limit;
			if(this.bid) {
				query += '&bid=' + this.bid;
			}
			if(this.ask) {
				query += '&ask=' + this.ask;
			}
			fetch(query)
				.then(response => response.json())
				.then(data => this.data = data);
		},
		confirm(offer) {
			this.offer = offer;
			$(this.$refs.confirm).modal('show');
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
							this.$emit('accept-offer', {offer: offer, result: data});
						});
					} else {
						response.text().then(data => {
							this.error = data;
							this.result = null;
						});
					}
				});
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
		<div class="ui message" v-if="result">
			Transaction has been sent: <router-link :to="'/explore/transaction/' + result">{{result}}</router-link>
		</div>
		<div class="ui negative message" v-if="error">
			Failed with: <b>{{error}}</b>
		</div>
		<table class="ui table striped">
			<thead>
				<th>They Offer</th>
				<th>They Ask</th>
				<template v-if="bid && ask">
					<th>Price</th>
				</template>
				<th>Time</th>
				<th>Link</th>
				<th></th>
			</thead>
			<tbody>
				<tr v-for="item in data">
					<td>
						<template v-for="(entry, index) in item.bids">
							<template v-if="index">, </template><b>{{entry.value}}</b> {{entry.symbol}}
						</template>
					</td>
					<td>
						<template v-for="(entry, index) in item.asks">
							<template v-if="index">, </template><b>{{entry.value}}</b> {{entry.symbol}}
						</template>
					</td>
					<template v-if="bid && ask">
						<td><b>{{item.price}}</b></td>
					</template>
					<td>{{new Date(item.time * 1000).toLocaleString()}}</td>
					<td><router-link :to="'/explore/address/' + item.address">Address</router-link></td>
					<td><div class="ui tiny compact button" @click="confirm(item)">Accept</div></td>
				</tr>
			</tbody>
		</table>
		<div class="ui modal" ref="confirm">
			<div class="header">Accept Offer</div>
			<div class="content">
				<table class="ui definition table striped">
				<tr>
					<td>Address</td>
					<td><router-link :to="'/explore/address/' + offer.address">{{offer.address}}</router-link></td>
				</tr>
				<tr>
					<template v-for="entry in offer.bids">
						<td>You Receive</td>
						<td>
							<b>{{entry.value}}</b> {{entry.symbol}} 
							<template v-if="entry.contract != 'mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev'">
								- [<router-link :to="'/explore/address/' + entry.contract">{{entry.contract}}</router-link>]
							</template>
						</td>
					</template>
				</tr>
				<tr>
					<template v-for="entry in offer.asks">
						<td>You Pay</td>
						<td>
							<b>{{entry.value}}</b> {{entry.symbol}} 
							<template v-if="entry.contract != 'mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev'">
								- [<router-link :to="'/explore/address/' + entry.contract">{{entry.contract}}</router-link>]
							</template>
						</td>
					</template>
				</tr>
				</table>
			</div>
			<div class="actions">
				<div class="ui positive approve button" @click="accept(offer)">Accept</div>
				<div class="ui negative cancel button">Cancel</div>
			</div>
		</div>
		`
})
