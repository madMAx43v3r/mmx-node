
app.component('exchange-menu', {
	props: {
		server_: String,
		bid_: String,
		ask_: String,
		page: String
	},
	emits: [
		"update-wallet", "update-bid-symbol", "update-ask-symbol"
	],
	data() {
		return {
			servers: [],
			server: null,
			wallets: [],
			wallet: null,
			pairs: [],
			bid_pairs: [],
			ask_pairs: [],
			bid_symbol_map: null,
			ask_symbol_map: null,
			bid: null,
			ask: null,
		}
	},
	methods: {
		update() {
			fetch('/api/exchange/get_servers')
				.then(response => response.json())
				.then(data => {
					this.servers = data;
					if(data) {
						this.server = data[0];
					}
				});
			fetch('/api/wallet/get_all_accounts')
				.then(response => response.json())
				.then(data => {
					this.wallets = data;
					if(data.length > 0) {
						this.wallet = data[0][0];
					}
				});
		},
		submit(page) {
			if(this.server && this.bid && this.ask) {
				if(!page) {
					if(this.page) {
						page = this.page;
					} else {
						page = "market";
					}
				}
				this.$router.push('/exchange/' + page + '/' + this.server + '/' + this.bid + '/' + this.ask);
				this.$emit('update-bid-symbol', this.bid_symbol_map.get(this.bid));
				this.$emit('update-ask-symbol', this.bid_symbol_map.get(this.ask));
			}
		}
	},
	created() {
		this.update();
		if(this.server_) {
			this.server = this.server_;
		}
	},
	watch: {
		server(value) {
			fetch('/wapi/exchange/pairs?server=' + value)
				.then(response => response.json())
				.then(data => {
					this.pairs = data;
					let list = [];
					let map = new Map();
					for(const item of data) {
						if(!map.has(item.bid)) {
							map.set(item.bid, item.bid_symbol);
							list.push({currency: item.bid, symbol: item.bid_symbol});
						}
					}
					this.bid_pairs = list;
					this.bid_symbol_map = map;
					if(!this.bid && this.bid_) {
						this.bid = this.bid_;
					}
					this.submit();
				});
		},
		wallet(value) {
			this.$emit('update-wallet', value);
		},
		bid(value) {
			let list = [];
			let map = new Map();
			for(const item of this.pairs) {
				if(item.bid == this.bid && !map.has(item.ask)) {
					map.set(item.ask, item.ask_symbol);
					list.push({currency: item.ask, symbol: item.ask_symbol});
				}
			}
			this.ask_pairs = list;
			this.ask_symbol_map = map;
			if(!this.ask && this.ask_) {
				this.ask = this.ask_;
			} else if(list.length == 1) {
				this.ask = list[0].currency;
			}
			this.submit();
		},
		ask(value) {
			this.submit();
		}
	},
	template: `
		<div class="ui form segment">
			<div class="two fields">
				<div class="field">
					<label>Server</label>
					<select v-model="server">
						<option v-for="item in servers" :key="item" :value="item">{{item}}</option>
					</select>
				</div>
				<div class="field">
					<label>Wallet</label>
					<select v-model="wallet">
						<option v-for="item in wallets" :key="item[0]" :value="item[0]">Account #{{item[0]}} ([{{item[1].index}}] {{item[1].name}})</option>
					</select>
				</div>
			</div>
			<div class="field">
				<label>Bid</label>
				<select v-model="bid">
					<option v-for="item in bid_pairs" :key="item.currency" :value="item.currency">
						{{item.symbol}}<template v-if="item.currency != 'mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev'"> - [{{item.currency}}]</template>
					</option>
				</select>
			</div>
			<div class="field">
				<label>Ask</label>
				<select v-model="ask">
					<option v-for="item in ask_pairs" :key="item.currency" :value="item.currency">
						{{item.symbol}}<template v-if="item.currency != 'mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev'"> - [{{item.currency}}]</template>
					</option>
				</select>
			</div>
		</div>
		<div class="ui large pointing menu">
			<a class="item" :class="{active: $route.meta.page == 'market'}" @click="submit('market')">Market</a>
			<a class="item" :class="{active: $route.meta.page == 'trades'}">Trades</a>
			<a class="item" :class="{active: $route.meta.page == 'history'}" @click="submit('history')">History</a>
			<a class="item" :class="{active: $route.meta.page == 'offers'}">Offers</a>
		</div>
		`
})

app.component('exchange-order-list', {
	props: {
		bid: String,
		ask: String,
		server: String,
		flip: Boolean,
		title: String,
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
			fetch('/wapi/exchange/orders?server=' + this.server + '&bid=' + this.bid + '&ask=' + this.ask + '&limit=' + this.limit)
				.then(response => response.json())
				.then(data => {
					this.data = data;
				});
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
				<th>{{title}} [{{flip ? data.bid_symbol : data.ask_symbol}} / {{flip ? data.ask_symbol : data.bid_symbol}}]</th>
				<th>Amount</th>
				<th>Token</th>
			</thead>
			<tbody>
				<tr v-for="item in data.orders">
					<td>{{flip ? item.inv_price : item.price}}</td>
					<td>{{flip ? item.ask_value : item.bid_value}}</td>
					<td>{{flip ? data.ask_symbol : data.bid_symbol}}</td>
				</tr>
			</tbody>
		</table>
		`
})

app.component('exchange-orders', {
	props: {
		bid: String,
		ask: String,
		server: String,
		limit: Number
	},
	methods: {
		update() {
			this.$refs.bid_list.update();
			this.$refs.ask_list.update();
		}
	},
	template: `
		<div class="ui two column grid">
			<div class="column">
				<exchange-order-list title="Buy -" :server="server" :bid="bid" :ask="ask" :flip="false" :limit="limit" ref="bid_list"></exchange-order-list>
			</div>
			<div class="column">
				<exchange-order-list title="Sell -" :server="server" :bid="ask" :ask="bid" :flip="true" :limit="limit" ref="ask_list"></exchange-order-list>
			</div>
		</div>
		`
})

app.component('exchange-history', {
	props: {
		bid: String,
		ask: String,
		limit: Number
	},
	data() {
		return {
			data: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/exchange/history?limit=' + this.limit + '&bid=' + this.bid + '&ask=' + this.ask)
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update();
	},
	template: `
		<table class="ui table striped">
			<thead>
				<th>Type</th>
				<th>Bid</th>
				<th>Token</th>
				<th>Ask</th>
				<th>Token</th>
				<th>Offer</th>
				<th>Height</th>
				<th>Time</th>
			</thead>
			<tbody>
				<tr v-for="item in data">
					<td>{{item.pair.bid == bid ? "SELL" : "BUY"}}</td>
					<td>{{item.pair.bid == bid ? item.bid_value : item.ask_value}}</td>
					<td>{{item.pair.bid == bid ? item.bid_symbol : item.ask_symbol}}</td>
					<td>{{item.pair.bid == bid ? item.ask_value : item.bid_value}}</td>
					<td>{{item.pair.bid == bid ? item.ask_symbol : item.bid_symbol}}</td>
					<td>{{item.offer_id}}</td>
					<td>{{item.failed ? "(failed)" : (item.height ? item.height : "(pending)")}}</td>
					<td>{{item.time ? new Date(item.time * 1000).toLocaleString() : "(pending)"}}</td>
				</tr>
			</tbody>
		</table>
		`
})

app.component('exchange-trade-form', {
	props: {
		server: String,
		wallet: Number,
		bid_symbol: String,
		ask_symbol: String,
		bid_currency: String,
		ask_currency: String,
	},
	emits: [
		"trade-executed"
	],
	data() {
		return {
			balance: null,
			bid_amount: null,
			ask_amount: null,
			confirmed: false,
			timer: null
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/balance?index=' + this.wallet + '&currency=' + this.bid_currency)
				.then(response => response.json())
				.then(data => {
					if(data.balances.length) {
						this.balance = data.balances[0].spendable;
					} else {
						this.balance = 0;
					}
				});
		},
		submit() {
			this.confirmed = false;
			const req = {};
			req.server = this.server;
			req.wallet = this.wallet;
			const pair = {};
			pair.bid = this.bid_currency;
			pair.ask = this.ask_currency;
			req.pair = pair;
			req.bid = this.bid_amount;
			fetch('/wapi/exchange/trade', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						response.json().then(data => {
							if(data.length) {
								alert("TX IDs: " + JSON.stringify(data));
								this.$emit('trade-executed', data);
								this.update();
							} else {
								alert("Trade failed! (Most likely no offers available or bid amount too low)")
							}
						});
					} else {
						response.text().then(data => {
							alert("Failed with: " + data)
						});
					}
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 10000);
	},
	mounted() {
		$('.ui.checkbox').checkbox();
	},
	unmounted() {
		clearInterval(this.timer);
	},
	watch: {
		wallet() {
			this.update();
		},
		bid_amount(value) {
			fetch('/wapi/exchange/price?server=' + this.server + '&bid=' + this.bid_currency + '&ask=' + this.ask_currency + '&amount=' + value)
				.then(response => {
					if(response.ok) {
						response.json().then(data => {
							this.ask_amount = (this.bid_amount * data.price).toPrecision(6);
						});
					} else {
						this.ask_amount = "???";
					}
				});
		}
	},
	template: `
		<div class="ui segment">
		<form class="ui form" id="form">
			<div class="two fields">
				<div class="field">
					<label>Bid Amount</label>
					<div class="ui right labeled input">
						<input type="text" v-model.number.lazy="bid_amount" placeholder="1.23" style="text-align: right"/>
						<div class="ui basic label">
							{{bid_symbol}}
						</div>
					</div>
				</div>
				<div class="field">
					<label>Receive (estimated)</label>
					<div class="ui right labeled input field">
						<input type="text" v-model="ask_amount" style="text-align: right" disabled/>
						<div class="ui basic label">
							{{ask_symbol}}
						</div>
					</div>
				</div>
			</div>
			<div class="inline field">
				<div class="ui checkbox">
					<input type="checkbox" v-model="confirmed">
					<label>Confirm</label>
				</div>
			</div>
			<div @click="submit" class="ui submit primary button" :class="{disabled: !confirmed}" id="submit">Trade</div>
		</form>
		<div class="ui bottom right attached large label">
			Balance: {{balance}} {{bid_symbol}}
		</div>
		</div>
		`
})


