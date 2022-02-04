
app.component('exchange-menu', {
	props: {
		server_: String,
		bid_: String,
		ask_: String
	},
	data() {
		return {
			servers: [],
			server: null,
			wallets: [],
			wallet: null,
			pairs: [],
			bid_pairs: [],
			ask_pairs: [],
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
		submit() {
			if(this.server && this.bid && this.ask) {
				this.$router.push('/exchange/market/' + this.server + '/' + this.bid + '/' + this.ask);
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
					let set = [];
					let list = [];
					for(const item of data) {
						if(set.indexOf(item.bid) < 0) {
							set.push(item.bid);
							list.push({currency: item.bid, symbol: item.bid_symbol});
						}
					}
					this.bid_pairs = list;
					if(!this.bid && this.bid_) {
						this.bid = this.bid_;
					}
					this.submit();
				});
		},
		bid(value) {
			let set = [];
			let list = [];
			for(const item of this.pairs) {
				if(item.bid == this.bid && set.indexOf(item.ask) < 0) {
					set.push(item.ask);
					list.push({currency: item.ask, symbol: item.ask_symbol});
				}
			}
			this.ask_pairs = list;
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
		<div class="ui form raised segment">
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
			data: []
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
		this.update()
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
					<td>
						{{flip ? item.inv_price : item.price}}
					</td>
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
		<div class="ui segment">
			<div class="ui two column grid">
				<div class="column">
					<exchange-order-list title="Buy" :server="server" :bid="bid" :ask="ask" :flip="false" :limit="limit" ref="bid_list"></exchange-order-list>
				</div>
				<div class="column">
					<exchange-order-list title="Sell" :server="server" :bid="ask" :ask="bid" :flip="true" :limit="limit" ref="ask_list"></exchange-order-list>
				</div>
			</div>
		</div>
		`
})




