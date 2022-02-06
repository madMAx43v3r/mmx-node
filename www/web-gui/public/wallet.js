
app.component('wallet-summary', {
	data() {
		return {
			data: []
		}
	},
	methods: {
		update() {
			fetch('/api/wallet/get_all_accounts')
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update()
	},
	template: `
		<div class="ui raised segment" v-for="item in data" :key="item[0]">
			<account-summary :index="item[0]" :account="item[1]"></account-summary>
		</div>
		`
})

app.component('account-menu', {
	props: {
		index: Number
	},
	template: `
		<div class="ui large pointing menu">
			<router-link class="item" :class="{active: $route.meta.page == 'balance'}" :to="'/wallet/account/' + index">Balance</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'nfts'}" :to="'/wallet/account/' + index + '/nfts'">NFTs</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'contracts'}" :to="'/wallet/account/' + index + '/contracts'">Contracts</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'addresses'}" :to="'/wallet/account/' + index + '/addresses'">Addresses</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'send'}" :to="'/wallet/account/' + index + '/send'">Send</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'offer'}" :to="'/wallet/account/' + index + '/offer'">Offer</router-link>
			<a class="item">Details</a>
			<a class="right item"><i class="cog icon"></i></a>
		</div>
		`
})

app.component('account-header', {
	props: {
		index: Number,
		account: Object
	},
	data() {
		return {
			info: {
				name: null,
				index: null
			},
			address: null
		}
	},
	methods: {
		update() {
			if(this.account) {
				this.info = this.account
			} else {
				fetch('/api/wallet/get_account?index=' + this.index)
					.then(response => response.json())
					.then(data => this.info = data);
			}
			fetch('/wapi/wallet/address?index=' + this.index)
				.then(response => response.json())
				.then(data => this.address = data[0]);
		}
	},
	created() {
		this.update()
	},
	template: `
		<div class="ui large labels">
			<div class="ui horizontal label">Account #{{index}}</div>
			<div class="ui horizontal label">[{{info.index}}] {{info.name}}</div>
			<div class="ui horizontal label">{{address}}</div>
		</div>
		`
})

app.component('account-summary', {
	props: {
		index: Number,
		account: Object
	},
	template: `
		<div>
			<account-header :index="index" :account="account"></account-header>
			<account-menu :index="index"></account-menu>
			<account-balance :index="index"></account-balance>
		</div>
		`
})

app.component('account-balance', {
	props: {
		index: Number
	},
	data() {
		return {
			balances: [],
			timer: null
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/balance?index=' + this.index)
				.then(response => response.json())
				.then(data => this.balances = data.balances);
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 30000);
	},
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<table class="ui table">
			<thead>
			<tr>
				<th class="two wide">Balance</th>
				<th class="two wide">Reserved</th>
				<th class="two wide">Spendable</th>
				<th class="two wide">Token</th>
				<th>Contract</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in balances" :key="item.contract">
				<td>{{item.total}}</td>
				<td>{{item.reserved}}</td>
				<td><b>{{item.spendable}}</b></td>
				<td>{{item.symbol}}</td>
				<td>{{item.is_native ? '' : item.contract}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('balance-table', {
	props: {
		balances: Object
	},
	template: `
		<table class="ui table">
			<thead>
			<tr>
				<th class="two wide">Balance</th>
				<th class="two wide">Token</th>
				<th>Contract</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in balances" :key="item.contract">
				<td><b>{{item.value}}</b></td>
				<td>{{item.symbol}}</td>
				<td>{{item.is_native ? '' : item.contract}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('nft-table', {
	props: {
		index: Number
	},
	data() {
		return {
			nfts: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/balance?index=' + this.index)
				.then(response => response.json())
				.then(data => this.nfts = data.nfts);
		}
	},
	created() {
		this.update()
	},
	template: `
		<table class="ui table">
			<thead>
			<tr>
				<th>NFT</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in nfts" :key="item">
				<td>{{item}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('account-history', {
	props: {
		index: Number,
		limit: Number
	},
	data() {
		return {
			data: [],
			timer: null
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/history?limit=' + this.limit + '&index=' + this.index)
				.then(response => response.json())
				.then(data => this.data = data);
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
		<table class="ui table striped">
			<thead>
			<tr>
				<th>Height</th>
				<th>Type</th>
				<th>Amount</th>
				<th>Token</th>
				<th>Address</th>
				<th>Time</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data">
				<td>{{item.height}}</td>
				<td>{{item.type}}</td>
				<td><b>{{item.value}}</b></td>
				<td>{{item.symbol}}</td>
				<td>{{item.address}}</td>
				<td>{{new Date(item.time * 1000).toLocaleString()}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('contract-summary', {
	props: {
		address: String,
		contract: Object
	},
	data() {
		return {
			balances: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/address?id=' + this.address)
				.then(response => response.json())
				.then(data => this.balances = data.balances);
		}
	},
	created() {
		this.update()
	},
	template: `
		<div class="ui raised segment">
			<div class="ui large labels">
				<div class="ui horizontal label">{{contract.__type}}</div>
				<div class="ui horizontal label">{{address}}</div>
			</div>
			<table class="ui definition table striped">
				<tbody>
				<template v-for="(value, key) in contract" :key="key">
					<tr v-if="key != '__type'">
						<td class="collapsing">{{key}}</td>
						<td>{{value}}</td>
					</tr>
				</template>
				</tbody>
			</table>
			<balance-table :balances="balances" v-if="balances.length"></balance-table>
		</div>
		`
})

app.component('account-contracts', {
	props: {
		index: Number
	},
	data() {
		return {
			data: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/contracts?index=' + this.index)
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update()
	},
	template: `
		<contract-summary v-for="item in data" :key="item[0]" :address="item[0]" :contract="item[1]"></contract-summary>
		`
})

app.component('account-addresses', {
	props: {
		index: Number,
		limit: Number
	},
	data() {
		return {
			data: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/address?limit=' + this.limit + '&index=' + this.index)
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update()
	},
	template: `
		<table class="ui table striped">
			<thead>
			<tr>
				<th>Index</th>
				<th>Address</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="(item, index) in data" :key="index">
				<td>{{index}}</td>
				<td>{{item}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('account-send-form', {
	props: {
		index: Number
	},
	data() {
		return {
			accounts: [],
			balances: [],
			amount: null,
			target: null,
			address: null,
			currency: null,
			confirmed: false,
			options: {
				split_output: 1
			},
			result: null,
			error: null
		}
	},
	methods: {
		update() {
			fetch('/api/wallet/get_all_accounts')
				.then(response => response.json())
				.then(data => {
					for(const entry of data) {
						const info = entry[1];
						info.account = entry[0];
						fetch('/wapi/wallet/address?limit=1&index=' + entry[0])
							.then(response => response.json())
							.then(data => {
								info.address = data[0];
								this.accounts.push(info);
								this.accounts.sort((a, b) => a.account - b.account);
							});
					}
				});
			fetch('/wapi/wallet/balance?index=' + this.index)
				.then(response => response.json())
				.then(data => this.balances = data.balances);
		},
		submit() {
			this.confirmed = false;
			const req = {};
			req.index = this.index;
			req.amount = this.amount;
			req.currency = this.currency;
			req.dst_addr = this.target;
			req.options = this.options;
			fetch('/wapi/wallet/send', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						response.json().then(data => {
							this.result = data;
						});
					} else {
						response.text().then(data => {
							this.error = data;
						});
					}
					this.update();
					this.$refs.balance.update();
				});
		}
	},
	created() {
		this.update();
	},
	mounted() {
		$('.ui.checkbox').checkbox();
	},
	watch: {
		address(value) {
			if(value) {
				this.target = value;
			} else {
				this.target = null;
			}
		},
		amount(value) {
			// TODO: validate
		},
		result(value) {
			if(value) {
				this.error = null;
			}
		},
		error(value) {
			if(value) {
				this.result = null;
			}
		}
	},
	template: `
		<account-balance :index="index" ref="balance"></account-balance>
		<div class="ui raised segment">
			<form class="ui form">
				<div class="field">
					<label>Destination</label>
					<select v-model="address">
						<option value="">Address Input</option>
						<option v-for="item in accounts" :key="item.account" class="item" :value="item.address">
							Account #{{item.account}} ([{{item.index}}] {{item.name}}) ({{item.address}})
						</option>
					</select>
				</div>
				<div class="two fields">
					<div class="fourteen wide field">
						<label>Destination Address</label>
						<input type="text" v-model="target" :disabled="address" placeholder="mmx1..."/>
					</div>
					<div class="two wide field">
						<label>Output Split</label>
						<input type="text" v-model.number="options.split_output" style="text-align: right"/>
					</div>
				</div>
				<div class="two fields">
					<div class="four wide field">
						<label>Amount</label>
						<input type="text" v-model.number="amount" placeholder="1.23" style="text-align: right"/>
					</div>
					<div class="twelve wide field">
						<label>Currency</label>
						<select v-model="currency">
							<option v-for="item in balances" :key="item.contract" class="item" :value="item.contract">
								{{item.symbol}} <template v-if="!item.is_native"> - [{{item.contract}}]</template>
							</option>
						</select>
					</div>
				</div>
				<div class="inline field">
					<div class="ui toggle checkbox">
						<input type="checkbox" class="hidden" v-model="confirmed">
						<label>Confirm</label>
					</div>
				</div>
				<div @click="submit" class="ui submit primary button" :class="{disabled: !confirmed}">Send</div>
			</form>
		</div>
		<div class="ui large message" :class="{hidden: !result}">
			Transaction has been sent: <b>{{result}}</b>
		</div>
		<div class="ui large negative message" :class="{hidden: !error}">
			Failed with: <b>{{error}}</b>
		</div>
		`
})

app.component('account-offer-form', {
	props: {
		index: Number
	},
	data() {
		return {
			balances: [],
			bid_amount: null,
			ask_amount: null,
			ask_symbol: "MMX",
			bid_currency: null,
			ask_currency: null,
			confirmed: false,
			timer: null,
			result: null,
			error: null
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/balance?index=' + this.index)
				.then(response => response.json())
				.then(data => this.balances = data.balances);
		},
		update_balance() {
			this.update();
			this.$refs.balance.update();
		},
		submit() {
			this.confirmed = false;
			const req = {};
			req.index = this.index;
			const pair = {};
			pair.bid = this.bid_currency;
			pair.ask = this.ask_currency;
			req.pair = pair;
			req.bid = this.bid_amount;
			req.ask = this.ask_amount;
			fetch('/wapi/exchange/offer', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						response.json().then(data => {
							fetch('/wapi/exchange/place?id=' + data.id)
								.then(response => {
										if(response.ok) {
											this.result = data;
											this.update();
											this.$refs.balance.update();
											this.$refs.offers.update();
										} else {
											response.text().then(data => {
												this.error = data;
											});
										}
									});
						});
					} else {
						response.text().then(data => {
							this.error = data;
						});
					}
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 30000);
	},
	mounted() {
		$('.ui.checkbox').checkbox();
	},
	unmounted() {
		clearInterval(this.timer);
	},
	watch: {
		ask_amount(value) {
			// TODO: validate
		},
		bid_amount(value) {
			// TODO: validate
		},
		ask_currency(value) {
			if(value) {
				fetch('/wapi/contract?id=' + value)
					.then(response => {
						if(response.ok) {
							response.json()
								.then(data => {
									this.ask_symbol = data.symbol;
								});
						} else {
							this.ask_symbol = "???";
						}
					});
			} else {
				this.ask_symbol = "MMX";
			}
		},
		result(value) {
			if(value) {
				this.error = null;
			}
		},
		error(value) {
			if(value) {
				this.result = null;
			}
		}
	},
	template: `
		<account-balance :index="index" ref="balance"></account-balance>
		<div class="ui raised segment">
			<form class="ui form" id="form">
				<div class="two fields">
					<div class="four wide field">
						<label>Bid Amount</label>
						<input type="text" v-model.number="bid_amount" placeholder="1.23" style="text-align: right"/>
					</div>
					<div class="twelve wide field">
						<label>Bid Currency</label>
						<select v-model="bid_currency">
							<option v-for="item in balances" :key="item.contract" class="item" :value="item.contract">
								{{item.symbol}} <template v-if="!item.is_native"> - [{{item.contract}}]</template>
							</option>
						</select>
					</div>
				</div>
				<div class="two fields">
					<div class="four wide field">
						<label>Ask Amount</label>
						<input type="text" v-model.number="ask_amount" placeholder="1.23" style="text-align: right"/>
					</div>
					<div class="two wide field">
						<label>Ask Symbol</label>
						<input type="text" v-model="ask_symbol" disabled/>
					</div>
					<div class="ten wide field">
						<label>Ask Currency Contract</label>
						<input type="text" v-model="ask_currency" placeholder="mmx1..."/>
					</div>
				</div>
				<div class="inline field">
					<div class="ui toggle checkbox">
						<input type="checkbox" class="hidden" v-model="confirmed">
						<label>Confirm</label>
					</div>
				</div>
				<div @click="submit" class="ui submit primary button" :class="{disabled: !confirmed}">Offer</div>
			</form>
		</div>
		<div class="ui large message" :class="{hidden: !result}">
			<template v-if="result">
				[<b>{{result.id}}</b>] Offering <b>{{result.bid_value}}</b> [{{result.bid_symbol}}] for <b>{{result.ask_value}}</b> [{{result.ask_symbol}}]
			</template>
		</div>
		<div class="ui large negative message" :class="{hidden: !error}">
			Failed with: <b>{{error}}</b>
		</div>
		<account-offers @offer-cancel="update_balance" :index="index" ref="offers"></account-offers>
		`
})

app.component('account-offers', {
	props: {
		index: Number,
		bid: String,
		ask: String
	},
	emits: [
		"offer-cancel"
	],
	data() {
		return {
			data: [],
			timer: null
		}
	},
	methods: {
		update() {
			fetch('/wapi/exchange/offers?wallet=' + this.index + (this.bid ? '&bid=' + this.bid : '') + (this.ask ? '&ask=' + this.ask : ''))
				.then(response => response.json())
				.then(data => this.data = data);
		},
		cancel(id) {
			fetch('/api/exchange/cancel_offer?id=' + id)
				.then(response => {
					this.update();
					this.$emit('offer-cancel', id);
				});
		}
	},
	watch: {
		index(value) {
			this.update();
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 30000);
	},
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<table class="ui table striped">
			<thead>
			<tr>
				<th>ID</th>
				<th>Bid</th>
				<th></th>
				<th>Ask</th>
				<th></th>
				<th>Price</th>
				<th></th>
				<th>Price</th>
				<th></th>
				<th>Status</th>
				<th>Actions</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data">
				<td>{{item.id}}</td>
				<td class="collapsing"><b>{{item.bid_value}}</b></td>
				<td>{{item.bid_symbol}}</td>
				<td class="collapsing"><b>{{item.ask_value}}</b></td>
				<td>{{item.ask_symbol}}</td>
				<td class="collapsing"><b>{{(item.ask_value / item.bid_value).toPrecision(5)}}</b></td>
				<td>{{item.ask_symbol}} / {{item.bid_symbol}}</td>
				<td class="collapsing"><b>{{(item.bid_value / item.ask_value).toPrecision(5)}}</b></td>
				<td>{{item.bid_symbol}} / {{item.ask_symbol}}</td>
				<td>{{(100 * item.bid_sold / item.bid).toPrecision(3)}} %</td>
				<td>
					<div class="ui tiny compact button" @click="cancel(item.id)">Cancel</div>
				</td>
			</tr>
			</tbody>
		</table>
		`
})
