
app.component('wallet-summary', {
	data() {
		return {
			data: null,
			loading: false
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/api/wallet/get_all_accounts')
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.data = data;
				});
		}
	},
	created() {
		this.update();
	},
	template: `
		<template v-if="!data && loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<div class="ui raised segment" v-for="item in data" :key="item[0]">
			<account-summary :index="item[0]" :account="item[1]"></account-summary>
		</div>
		<router-link to="/wallet/create">
			<button class="ui button">New Wallet</button>
		</router-link>
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
			<router-link class="item" :class="{active: $route.meta.page == 'split'}" :to="'/wallet/account/' + index + '/split'">Split</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'history'}" :to="'/wallet/account/' + index + '/history'">History</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'log'}" :to="'/wallet/account/' + index + '/log'">Log</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'details'}" :to="'/wallet/account/' + index + '/details'">Details</router-link>
			<router-link class="right item" :class="{active: $route.meta.page == 'options'}" :to="'/wallet/account/' + index + '/options'"><i class="cog icon"></i></router-link>
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
			<div class="ui horizontal label">Wallet #{{index}}</div>
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
			data: null,
			loading: false,
			timer: null
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/wallet/balance?index=' + this.index)
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.data = data.balances;
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
		<template v-if="!data && loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<table class="ui table" v-if="data">
			<thead>
			<tr>
				<th class="two wide">Balance</th>
				<th class="two wide">Reserved</th>
				<th class="two wide">Spendable</th>
				<th class="two wide">Token</th>
				<th>Contract</th>
				<th>More</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data" :key="item.contract">
				<td>{{item.total}}</td>
				<td>{{item.reserved}}</td>
				<td><b>{{item.spendable}}</b></td>
				<td>{{item.symbol}}</td>
				<td><router-link :to="'/explore/address/' + item.contract">{{item.is_native ? '' : item.contract}}</router-link></td>
				<td>
					<router-link :to="'/wallet/account/' + index + '/coins/' + item.contract">Coins</router-link>
				</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('balance-table', {
	props: {
		address: String,
		show_empty: Boolean
	},
	data() {
		return {
			data: null,
			loading: false,
			timer: null
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/balance?id=' + this.address)
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.data = data.balances;
				});
		}
	},
	watch: {
		address() {
			this.update();
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 10000);
	},
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<template v-if="!data && loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<table class="ui table" v-if="data && (data.length || show_empty)">
			<thead>
			<tr>
				<th class="two wide">Balance</th>
				<th class="two wide">Locked</th>
				<th class="two wide">Spendable</th>
				<th class="two wide">Token</th>
				<th>Contract</th>
				<th>More</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data" :key="item.contract">
				<td>{{item.total}}</td>
				<td>{{item.locked}}</td>
				<td><b>{{item.spendable}}</b></td>
				<td>{{item.symbol}}</td>
				<td><router-link :to="'/explore/address/' + item.contract">{{item.is_native ? '' : item.contract}}</router-link></td>
				<td><router-link :to="'/explore/address/coins/' + address + '/' + item.contract">Coins</router-link></td>
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
			data: null,
			loading: false,
			timer: null
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/wallet/history?limit=' + this.limit + '&index=' + this.index)
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.data = data;
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
		<template v-if="!data && loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<table class="ui table striped" v-if="data">
			<thead>
			<tr>
				<th>Height</th>
				<th>Type</th>
				<th>Amount</th>
				<th>Token</th>
				<th>Address</th>
				<th>Link</th>
				<th>Time</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data">
				<td><router-link :to="'/explore/block/height/' + item.height">{{item.height}}</router-link></td>
				<td>{{item.type}}</td>
				<td><b>{{item.value}}</b></td>
				<template v-if="item.is_native">
					<td>{{item.symbol}}</td>
				</template>
				<template v-if="!item.is_native">
					<td><router-link :to="'/explore/address/' + item.contract">{{item.symbol}}</router-link></td>
				</template>
				<td><router-link :to="'/explore/address/' + item.address">{{item.address}}</router-link></td>
				<td><router-link :to="'/explore/transaction/' + item.txid">TX</router-link></td>
				<td>{{new Date(item.time * 1000).toLocaleString()}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('account-tx-history', {
	props: {
		index: Number,
		limit: Number
	},
	data() {
		return {
			data: null,
			loading: false,
			timer: null
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/wallet/tx_history?limit=' + this.limit + '&index=' + this.index)
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.data = data;
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 10000);
	},
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<template v-if="!data && loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<table class="ui table striped" v-if="data">
			<thead>
			<tr>
				<th>Height</th>
				<th>Confirmed</th>
				<th>Transaction ID</th>
				<th>Time</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data" :key="item.txid">
				<td>{{item.height ? item.height : "pending"}}</td>
				<td>{{item.confirm ? item.confirm : 0}}</td>
				<td><router-link :to="'/explore/transaction/' + item.id">{{item.id}}</router-link></td>
				<td>{{new Date(item.time).toLocaleString()}}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('account-contract-summary', {
	props: {
		index: Number,
		address: String,
		contract: Object
	},
	methods: {
		deposit() {
			this.$router.push("/wallet/account/" + this.index + "/send/" + this.address);
		},
		withdraw() {
			this.$router.push("/wallet/account/" + this.index + "/send_from/" + this.address);
		}
	},
	template: `
		<div class="ui raised segment">
			<div class="ui large labels">
				<div class="ui horizontal label">{{contract.__type}}</div>
				<div class="ui horizontal label">{{address}}</div>
			</div>
			<object-table :data="contract"></object-table>
			<balance-table :address="address"></balance-table>
			<div @click="deposit" class="ui submit button">Deposit</div>
			<div @click="withdraw" class="ui submit button">Withdraw</div>
		</div>
		`
})

app.component('account-contracts', {
	props: {
		index: Number
	},
	data() {
		return {
			data: null,
			loading: false
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/wallet/contracts?index=' + this.index)
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.data = data;
				});
		}
	},
	created() {
		this.update();
	},
	template: `
		<template v-if="!data && loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<template v-if="data">
			<account-contract-summary v-for="item in data" :key="item[0]" :index="index" :address="item[0]" :contract="item[1]"></account-contract-summary>
		</template>
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
		<table class="ui definition table striped">
			<thead>
			<tr>
				<th>Index</th>
				<th>Address</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="(item, index) in data" :key="index">
				<td>{{index}}</td>
				<td><router-link :to="'/explore/address/' + item">{{item}}</router-link></td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('account-coins', {
	props: {
		index: Number,
		currency: String,
		limit: Number
	},
	data() {
		return {
			data: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/coins?limit=' + this.limit + '&index=' + this.index + '&currency=' + this.currency)
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update()
	},
	template: `
		<coins-table :data="data"></coins-table>
		`
})

app.component('address-coins', {
	props: {
		address: String,
		currency: String,
		limit: Number
	},
	data() {
		return {
			data: []
		}
	},
	methods: {
		update() {
			fetch('/wapi/address/coins?limit=' + this.limit + '&id=' + this.address + '&currency=' + this.currency)
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update()
	},
	template: `
		<div class="ui large labels">
			<div class="ui horizontal label">Coins</div>
			<div class="ui horizontal label">{{address}}</div>
		</div>
		<coins-table :data="data"></coins-table>
		`
})

app.component('coins-table', {
	props: {
		data: Object
	},
	template: `
		<table class="ui table striped">
			<thead>
			<tr>
				<th>Height</th>
				<th>Amount</th>
				<th></th>
				<th>Address</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data" :key="item.key">
				<td>{{item.output.height == 4294967295 ? "pending" : item.output.height}}</td>
				<td class="collapsing"><b>{{item.output.value}}</b></td>
				<td><router-link :to="'/explore/address/' + item.output.contract">{{item.output.symbol}}</router-link></td>
				<td><router-link :to="'/explore/address/' + item.output.address">{{item.output.address}}</router-link></td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('account-details', {
	props: {
		index: Number
	},
	data() {
		return {
			account: null,
			keys: null
		}
	},
	methods: {
		update() {
			fetch('/api/wallet/get_account?index=' + this.index)
				.then(response => response.json())
				.then(data => this.account = data);
			fetch('/wapi/wallet/keys?index=' + this.index)
				.then(response => response.json())
				.then(data => this.keys = data);
		}
	},
	created() {
		this.update();
	},
	template: `
		<object-table :data="account"></object-table>
		<object-table :data="keys"></object-table>
		`
})

app.component('account-actions', {
	props: {
		index: Number
	},
	data() {
		return {
			seed: null,
			info: null,
			error: null
		}
	},
	methods: {
		reset_cache() {
			const req = {};
			req.index = this.index;
			fetch('/api/wallet/reset_cache', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						this.info = "Success";
					} else {
						response.text().then(data => {
							this.error = data;
						});
					}
				});
		},
		show_seed() {
			fetch('/wapi/wallet/seed?index=' + this.index)
				.then(response => response.json())
				.then(data => {
					this.seed = data;
					$(this.$refs.seed_modal).modal('show');
				});
		}
	},
	template: `
		<div class="ui raised segment">
			<div @click="reset_cache" class="ui button">Reset Cache</div>
			<div @click="show_seed" class="ui button">Show Seed</div>
		</div>
		<div class="ui message" :class="{hidden: !info}">
			<b>{{info}}</b>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			Failed with: <b>{{error}}</b>
		</div>
		<div class="ui modal" ref="seed_modal">
			<div class="content">
				<object-table :data="seed"></object-table>
			</div>
		</div>
		`
})

app.component('create-account', {
	props: {
		index: Number
	},
	data() {
		return {
			data: null,
			name: null,
			offset: null,
			num_addresses: 100,
			error: null
		}
	},
	methods: {
		update() {
			fetch('/api/wallet/get_account?index=' + this.index)
				.then(response => response.json())
				.then(data => this.data = data);
		},
		submit() {
			if(this.offset < 1) {
				this.error = "'Account Index' cannot be less than 1";
				return;
			}
			const req = {};
			req.config = {};
			req.config.name = this.name;
			req.config.index = this.offset;
			req.config.key_file = this.data.key_file;
			req.config.num_addresses = this.num_addresses;
			fetch('/api/wallet/create_account', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						this.$router.push('/wallet/');
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
	},
	template: `
		<div class="ui raised segment">
			<form class="ui form">
				<div class="three fields">
					<div class="three wide field">
						<label>Account Index</label>
						<input type="text" v-model.number="offset" style="text-align: right"/>
					</div>
					<div class="nine wide field">
						<label>Account Name</label>
						<input type="text" v-model="name"/>
					</div>
					<div class="four wide field">
						<label>Number of Addresses</label>
						<input type="text" v-model.number="num_addresses" style="text-align: right"/>
					</div>
				</div>
				<div @click="submit" class="ui submit primary button">Create Account</div>
			</form>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			Failed with: <b>{{error}}</b>
		</div>
		`
})

app.component('create-wallet', {
	data() {
		return {
			name: null,
			num_addresses: 100,
			seed: null,
			error: null
		}
	},
	methods: {
		submit() {
			if(this.seed) {
				if(this.seed == "") {
					this.seed = null;
				}
				else if(this.seed.length != 64) {
					this.error = "invalid seed value";
					return;
				}
			}
			const req = {};
			req.config = {};
			req.config.name = this.name;
			req.config.num_addresses = this.num_addresses;
			req.seed = this.seed;
			fetch('/api/wallet/create_wallet', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						this.$router.push('/wallet/');
					} else {
						response.text().then(data => {
							this.error = data;
						});
					}
				});
		}
	},
	template: `
		<div class="ui raised segment">
			<form class="ui form">
				<div class="two fields">
					<div class="twelve wide field">
						<label>Account Name</label>
						<input type="text" v-model="name"/>
					</div>
					<div class="four wide field">
						<label>Number of Addresses</label>
						<input type="text" v-model.number="num_addresses" style="text-align: right"/>
					</div>
				</div>
				<div class="field">
					<label>Seed Hash (optional, hex string, 64 chars)</label>
					<input type="text" v-model="seed" placeholder="<random>"/>
				</div>
				<div @click="submit" class="ui submit primary button">Create Wallet</div>
			</form>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			Failed with: <b>{{error}}</b>
		</div>
		`
})

app.component('account-send-form', {
	props: {
		index: Number,
		target_: String,
		source_: String
	},
	data() {
		return {
			accounts: [],
			balances: [],
			amount: null,
			target: null,
			source: null,
			address: "",
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
			if(this.source) {
				fetch('/wapi/address?id=' + this.source)
					.then(response => response.json())
					.then(data => this.balances = data.balances);
			} else {
				fetch('/wapi/wallet/balance?index=' + this.index)
					.then(response => response.json())
					.then(data => this.balances = data.balances);
			}
		},
		submit() {
			this.confirmed = false;
			const req = {};
			req.index = this.index;
			req.amount = this.amount;
			req.currency = this.currency;
			if(this.source) {
				req.src_addr = this.source;
			}
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
					this.$refs.history.update();
				});
		}
	},
	created() {
		if(this.source_) {
			this.source = this.source_;
		}
		if(this.target_) {
			this.address = "";
			this.target = this.target_;
		}
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
		<balance-table :address="source" :show_empty="true" v-if="source" ref="balance"></balance-table>
		<account-balance :index="index" v-if="!source" ref="balance"></account-balance>
		<div class="ui raised segment">
			<form class="ui form">
				<div class="field" v-if="!!source">
					<label>Source Address</label>
					<input type="text" v-model="source" disabled/>
				</div>
				<div class="field">
					<label>Destination</label>
					<select v-model="address" :disabled="!!target_">
						<option value="">Address Input</option>
						<option v-for="item in accounts" :key="item.account" :value="item.address">
							Wallet #{{item.account}} ({{item.address}})
						</option>
					</select>
				</div>
				<div class="two fields">
					<div class="fourteen wide field">
						<label>Destination Address</label>
						<input type="text" v-model="target" :disabled="!!address || !!target_" placeholder="mmx1..."/>
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
							<option v-for="item in balances" :key="item.contract" :value="item.contract">
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
		<div class="ui message" :class="{hidden: !result}">
			Transaction has been sent: <router-link :to="'/explore/transaction/' + result">{{result}}</router-link>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			Failed with: <b>{{error}}</b>
		</div>
		<account-tx-history :index="index" :limit="10" ref="history"></account-tx-history>
		`
})

app.component('account-split-form', {
	props: {
		index: Number
	},
	data() {
		return {
			balances: [],
			amount: null,
			currency: null,
			confirmed: false,
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
		submit() {
			this.confirmed = false;
			const req = {};
			req.index = this.index;
			req.amount = this.amount;
			req.currency = this.currency;
			fetch('/wapi/wallet/split', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						response.json().then(data => {
							if(data && data != "null") {
								this.result = data;
							} else {
								this.error = "nothing to split";
							}
						});
					} else {
						response.text().then(data => {
							this.error = data;
						});
					}
					this.update();
					this.$refs.balance.update();
					this.$refs.history.update();
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
				<div class="two fields">
					<div class="four wide field">
						<label>Max Coin Amount</label>
						<input type="text" v-model.number="amount" placeholder="1.23" style="text-align: right"/>
					</div>
					<div class="twelve wide field">
						<label>Currency</label>
						<select v-model="currency">
							<option v-for="item in balances" :key="item.contract" :value="item.contract">
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
		<div class="ui message" :class="{hidden: !result}">
			Transaction has been sent: <router-link :to="'/explore/transaction/' + result">{{result}}</router-link>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			Failed with: <b>{{error}}</b>
		</div>
		<account-tx-history :index="index" :limit="10" ref="history"></account-tx-history>
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
			num_chunks: 1,
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
			req.num_chunks = this.num_chunks;
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
				<div class="three fields">
					<div class="four wide field">
						<label>Offer Amount</label>
						<input type="text" v-model.number="bid_amount" placeholder="1.23" style="text-align: right"/>
					</div>
					<div class="ten wide field">
						<label>Offer Currency</label>
						<select v-model="bid_currency">
							<option v-for="item in balances" :key="item.contract" :value="item.contract">
								{{item.symbol}} <template v-if="!item.is_native"> - [{{item.contract}}]</template>
							</option>
						</select>
					</div>
					<div class="two wide field">
						<label>No. Chunks</label>
						<input type="text" v-model.number="num_chunks" style="text-align: right"/>
					</div>
				</div>
				<div class="two fields">
					<div class="four wide field">
						<label>Receive Amount</label>
						<input type="text" v-model.number="ask_amount" placeholder="1.23" style="text-align: right"/>
					</div>
					<div class="two wide field">
						<label>Symbol</label>
						<input type="text" v-model="ask_symbol" disabled/>
					</div>
					<div class="ten wide field">
						<label>Receive Currency Contract</label>
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
		<div class="ui message" :class="{hidden: !result}">
			<template v-if="result">
				[<b>{{result.id}}</b>] Offering <b>{{result.bid_value}}</b> [{{result.bid_symbol}}] for <b>{{result.ask_value}}</b> [{{result.ask_symbol}}]
			</template>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
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
				<template v-if="bid && ask">
					<th>Type</th>
				</template>
				<th colspan="2">Amount</th>
				<th colspan="2">Receive</th>
				<th colspan="2">Price</th>
				<th colspan="2">Price</th>
				<th>Status</th>
				<th>Actions</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data" :key="item.id">
				<td>{{item.id}}</td>
				<template v-if="bid && ask">
					<td :class="item.type == 'BUY' ? 'positive' : 'negative'">{{item.type}}</td>
				</template>
				<td class="collapsing"><b>{{item.bid_value}}</b></td>
				<td>{{item.bid_symbol}}</td>
				<td class="collapsing"><b>{{item.ask_value}}</b></td>
				<td>{{item.ask_symbol}}</td>
				<td class="collapsing"><b>{{(item.ask_value / item.bid_value).toPrecision(5)}}</b></td>
				<td>{{item.ask_symbol}} / {{item.bid_symbol}}</td>
				<td class="collapsing"><b>{{(item.bid_value / item.ask_value).toPrecision(5)}}</b></td>
				<td>{{item.bid_symbol}} / {{item.ask_symbol}}</td>
				<td :class="{positive: item.bid_sold >= item.bid}">{{(100 * item.bid_sold / item.bid).toPrecision(3)}} %</td>
				<td>
					<div class="ui tiny compact button" @click="cancel(item.id)">{{item.bid_sold < item.bid ? 'Cancel' : 'Delete'}}</div>
				</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('create-contract-menu', {
	props: {
		index: Number
	},
	data() {
		return {
			type: null
		}
	},
	methods: {
		submit() {
			this.$router.push("/wallet/account/" + this.index + "/create/" + this.type);
		}
	},
	template: `
		<div class="ui raised segment">
			<form class="ui form">
				<div class="field">
					<label>Contract Type</label>
					<select v-model="type">
						<option value="staking">mmx.contract.Staking</option>
						<option value="locked">mmx.contract.Locked</option>
					</select>
				</div>
				<div @click="submit" class="ui submit button" :class="{disabled: !type}">Create</div>
			</form>
		</div>
		`
})

app.component('create-staking-contract', {
	props: {
		index: Number
	},
	data() {
		return {
			addresses: [],
			owner: null,
			currency: null,
			symbol: null,
			reward_addr: null,
			confirmed: false,
			valid: false,
			result: null,
			error: null
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/address?limit=100&index=' + this.index)
				.then(response => response.json())
				.then(data => {
					this.addresses = data;
					if(data.length) {
						if(!this.owner) {
							this.owner = data[0];
						}
						if(!this.reward_addr) {
							this.reward_addr = data[0];
						}
					}
				});
		},
		submit() {
			this.confirmed = false;
			const contract = {};
			contract.__type = "mmx.contract.Staking";
			contract.owner = this.owner;
			contract.currency = this.currency;
			contract.reward_addr = this.reward_addr;
			fetch('/wapi/wallet/deploy?index=' + this.index, {body: JSON.stringify(contract), method: "post"})
				.then(response => {
					if(response.ok) {
						response.json().then(data => this.result = data);
					} else {
						response.text().then(data => this.error = data);
					}
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
		currency(value) {
			this.valid = false;
			this.confirmed = false;
			this.symbol = "???";
			fetch('/wapi/contract?id=' + value)
				.then(response => {
					if(response.ok) {
						response.json().then(data => {
								if(data) {
									this.valid = true;
									this.symbol = data.symbol;
								}
							});
					}
				});
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
		<div class="ui large label">Create</div>
		<div class="ui large label">mmx.contract.Staking</div>
		<div class="ui raised segment">
			<form class="ui form" id="form">
				<div class="field">
					<label>Owner Address</label>
					<select v-model="owner">
						<option v-for="(item, index) in addresses" :key="item" :value="item">
							[{{index}}] {{item}}
						</option>
					</select>
				</div>
				<div class="field">
					<label>Reward Address</label>
					<select v-model="reward_addr">
						<option v-for="(item, index) in addresses" :key="item" :value="item">
							[{{index}}] {{item}}
						</option>
					</select>
				</div>
				<div class="two fields">
					<div class="two wide field">
						<label>Symbol</label>
						<input type="text" v-model="symbol" disabled/>
					</div>
					<div class="fourteen wide field">
						<label>Currency Contract</label>
						<input type="text" v-model="currency" placeholder="mmx1..."/>
					</div>
				</div>
				<div class="inline field">
					<div class="ui toggle checkbox" :class="{disabled: !valid}">
						<input type="checkbox" class="hidden" v-model="confirmed">
						<label>Confirm</label>
					</div>
				</div>
				<div @click="submit" class="ui submit primary button" :class="{disabled: !confirmed}">Deploy</div>
			</form>
		</div>
		<div class="ui message" :class="{hidden: !result}">
			<template v-if="result">
				Deployed as: <b>{{result}}</b>
			</template>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			Failed with: <b>{{error}}</b>
		</div>
		`
})

app.component('create-locked-contract', {
	props: {
		index: Number
	},
	data() {
		return {
			owner: null,
			chain_height: null,
			delta_height: null,
			valid: false,
			confirmed: false,
			result: null,
			error: null
		}
	},
	methods: {
		check_valid() {
			this.valid = this.owner && (this.chain_height || this.delta_height);
			if(!this.valid) {
				this.confirmed = false;
			}
		},
		submit() {
			this.confirmed = false;
			const contract = {};
			contract.__type = "mmx.contract.Locked";
			contract.owner = this.owner;
			contract.chain_height = this.chain_height;
			contract.delta_height = this.delta_height;
			fetch('/wapi/wallet/deploy?index=' + this.index, {body: JSON.stringify(contract), method: "post"})
				.then(response => {
					if(response.ok) {
						response.json().then(data => this.result = data);
					} else {
						response.text().then(data => this.error = data);
					}
				});
		}
	},
	mounted() {
		$('.ui.checkbox').checkbox();
	},
	watch: {
		owner(value) {
			this.check_valid();
		},
		chain_height(value) {
			this.check_valid();
		},
		delta_height(value) {
			this.check_valid();
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
		<div class="ui large label">Create</div>
		<div class="ui large label">mmx.contract.Locked</div>
		<div class="ui raised segment">
			<form class="ui form" id="form">
				<div class="field">
					<label>Owner Address</label>
					<input type="text" v-model="owner" placeholder="mmx1..."/>
				</div>
				<div class="field">
					<label>Unlock at Chain Height</label>
					<input type="text" v-model.number="chain_height"/>
				</div>
				<div class="field">
					<label>Unlock after N Blocks (starting at time of deposit)</label>
					<input type="text" v-model.number="delta_height"/>
				</div>
				<div class="inline field">
					<div class="ui toggle checkbox" :class="{disabled: !valid}">
						<input type="checkbox" class="hidden" v-model="confirmed">
						<label>Confirm</label>
					</div>
				</div>
				<div @click="submit" class="ui submit primary button" :class="{disabled: !confirmed}">Deploy</div>
			</form>
		</div>
		<div class="ui message" :class="{hidden: !result}">
			<template v-if="result">
				Deployed as: <b>{{result}}</b>
			</template>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			Failed with: <b>{{error}}</b>
		</div>
		`
})
