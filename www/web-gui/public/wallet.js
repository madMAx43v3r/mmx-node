
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
			<button class="ui button">{{ $t('wallet_summary.new_wallet') }}</button>
		</router-link>
		`
})

app.component('account-menu', {
	props: {
		index: Number
	},
	template: `
		<div class="ui large pointing menu">
			<router-link class="item" :class="{active: $route.meta.page == 'balance'}" :to="'/wallet/account/' + index">{{ $t('account_menu.balance') }}</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'nfts'}" :to="'/wallet/account/' + index + '/nfts'">{{ $t('account_menu.nfts') }}</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'contracts'}" :to="'/wallet/account/' + index + '/contracts'">{{ $t('account_menu.contracts') }}</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'addresses'}" :to="'/wallet/account/' + index + '/addresses'">{{ $t('account_menu.addresses') }}</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'send'}" :to="'/wallet/account/' + index + '/send'">{{ $t('account_menu.send') }}</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'offer'}" :to="'/wallet/account/' + index + '/offer'">{{ $t('account_menu.offer') }}</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'history'}" :to="'/wallet/account/' + index + '/history'">{{ $t('account_menu.history') }}</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'log'}" :to="'/wallet/account/' + index + '/log'">{{ $t('account_menu.log') }}</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'details'}" :to="'/wallet/account/' + index + '/details'">{{ $t('account_menu.details') }}</router-link>
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
			<div class="ui horizontal label">{{ $t('account_header.wallet') }} #{{index}}</div>
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
		this.timer = setInterval(() => { this.update(); }, 10000);
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
				<th class="two wide">{{ $t('account_balance.balance') }}</th>
				<th class="two wide">{{ $t('account_balance.reserved') }}</th>
				<th class="two wide">{{ $t('account_balance.spendable') }}</th>
				<th class="two wide">{{ $t('account_balance.token') }}</th>
				<th>{{ $t('account_balance.contract') }}</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data" :key="item.contract">
				<td>{{item.total}}</td>
				<td>{{item.reserved}}</td>
				<td><b>{{item.spendable}}</b></td>
				<td>{{item.symbol}}</td>
				<td><router-link :to="'/explore/address/' + item.contract">{{item.is_native ? '' : item.contract}}</router-link></td>
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
				<th class="two wide">{{ $t('balance_table.balance') }}</th>
				<th class="two wide">{{ $t('balance_table.locked') }}</th>
				<th class="two wide">{{ $t('balance_table.spendable') }}</th>
				<th class="two wide">{{ $t('balance_table.token') }}</th>
				<th>{{ $t('balance_table.contract') }}</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data" :key="item.contract">
				<td>{{item.total}}</td>
				<td>{{item.locked}}</td>
				<td><b>{{item.spendable}}</b></td>
				<td>{{item.symbol}}</td>
				<td><router-link :to="'/explore/address/' + item.contract">{{item.is_native ? '' : item.contract}}</router-link></td>
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
				<td><router-link :to="'/explore/address/' + item">{{item}}</router-link></td>
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
		<table class="ui compact table striped" v-if="data">
			<thead>
			<tr>
				<th>{{ $t('account_history.height') }}</th>
				<th>{{ $t('account_history.type') }}</th>
				<th>{{ $t('account_history.amount') }}</th>
				<th>{{ $t('account_history.token') }}</th>
				<th>{{ $t('account_history.address') }}</th>
				<th>{{ $t('account_history.link') }}</th>
				<th>{{ $t('account_history.time') }}</th>
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
					<td><router-link :to="'/explore/address/' + item.contract">{{item.is_nft ? "[NFT]" : item.symbol}}</router-link></td>
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
		<table class="ui compact table striped" v-if="data">
			<thead>
			<tr>
				<th>{{ $t('account_tx_history.height') }}</th>
				<th>{{ $t('account_tx_history.confirmed') }}</th>
				<th>{{ $t('account_tx_history.transaction_id') }}</th>
				<th>{{ $t('account_tx_history.time') }}</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data" :key="item.txid">
				<template v-if="item.height">
					<td><router-link :to="'/explore/block/height/' + item.height">{{item.height}}</router-link></td>
				</template>
				<template v-else>
					<td><i>{{ $t('account_tx_history.pending') }}</i></td>
				</template>
				<td>{{item.confirm ? item.confirm > 1000 ? "> 1000" : item.confirm : 0}}</td>
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
			<div @click="deposit" class="ui submit button">{{ $t('account_contract_summary.deposit') }}</div>
			<div @click="withdraw" class="ui submit button">{{ $t('account_contract_summary.withdraw') }}</div>
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
		<table class="ui compact definition table striped">
			<thead>
			<tr>
				<th>{{ $t('account_addresses.index') }}</th>
				<th>{{ $t('account_addresses.address') }}</th>
				<th>{{ $t('account_addresses.n_recv') }}</th>
				<th>{{ $t('account_addresses.n_spend') }}</th>
				<th>{{ $t('account_addresses.last_recv') }}</th>
				<th>{{ $t('account_addresses.last_spend') }}</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="(item, index) in data" :key="index">
				<td>{{index}}</td>
				<td><router-link :to="'/explore/address/' + item">{{item}}</router-link></td>
				<td></td>
				<td></td>
				<td></td>
				<td></td>
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
		},
		copyKeysToPlotter() {
			window.mmx.copyKeysToPlotter(JSON.stringify(this.keys))
		}
	},
	created() {
		this.update();
	},
	template: `
		<object-table :data="account"></object-table>
		<object-table :data="keys"></object-table>

		<div v-if="isWinGUI && this.keys" @click="copyKeysToPlotter" class="ui submit primary button">{{ $t('account_details.copy_keys_to_plotter') }}</div>
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
			<div @click="reset_cache" class="ui button">{{ $t('account_actions.reset_cache') }}</div>
			<div @click="show_seed" class="ui button">{{ $t('account_actions.show_seed') }}</div>
		</div>
		<div class="ui message" :class="{hidden: !info}">
			<b>{{info}}</b>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			{{ $t('common.failed_with') }}: <b>{{error}}</b>
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
						<label>{{ $t('create_account.account_index') }}</label>
						<input type="text" v-model.number="offset" style="text-align: right"/>
					</div>
					<div class="nine wide field">
						<label>{{ $t('create_account.account_name') }}</label>
						<input type="text" v-model="name"/>
					</div>
					<div class="four wide field">
						<label>{{ $t('create_account.number_of_addresses') }}</label>
						<input type="text" v-model.number="num_addresses" style="text-align: right"/>
					</div>
				</div>
				<div @click="submit" class="ui submit primary button">{{ $t('create_account.create_account') }}</div>
			</form>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			{{ $t('common.failed_with') }}: <b>{{error}}</b>
		</div>
		`
})

app.component('create-wallet', {
	data() {
		return {
			name: null,
			num_addresses: 100,
			with_seed: false,
			seed: null,
			error: null
		}
	},
	methods: {
		submit() {
			const req = {};
			req.config = {};
			req.config.name = this.name;
			req.config.num_addresses = this.num_addresses;
			if(this.with_seed) {
				if(this.seed.length != 64) {
					this.error = "invalid seed value";
					return;
				}
				req.seed = this.seed;
			}
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
	mounted() {
		$('.ui.checkbox').checkbox();
	},
	template: `
		<div class="ui raised segment">
			<form class="ui form">
				<div class="two fields">
					<div class="twelve wide field">
						<label>{{ $t('create_wallet.account_name') }}</label>
						<input type="text" v-model="name"/>
					</div>
					<div class="four wide field">
						<label>{{ $t('create_wallet.number_of_addresses') }}</label>
						<input type="text" v-model.number="num_addresses" style="text-align: right"/>
					</div>
				</div>
				<div class="inline field">
					<div class="ui toggle checkbox">
						<input type="checkbox" class="hidden" v-model="with_seed">
						<label>{{ $t('create_wallet.use_custom_seed') }}</label>
					</div>
				</div>
				<div class="field">
					<label>{{ $t('create_wallet.seed_hash') }}</label>
					<input type="text" v-model="seed" :placeholder="$t('create_wallet.placeholder')" :disabled="!with_seed"/>
				</div>
				<div @click="submit" class="ui submit primary button">{{ $t('create_wallet.create_wallet') }}</div>
			</form>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			{{ $t('common.failed_with') }}: <b>{{error}}</b>
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
			result: null,
			error: null
		}
	},
	methods: {
		update() {
			fetch('/api/wallet/get_all_accounts')
				.then(response => response.json())
				.then(data => {
					this.accounts = [];
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
			if(!validate_address(this.target)) {
				this.error = "invalid destination address";
				return;
			}
			const req = {};
			req.index = this.index;
			req.amount = this.amount;
			req.currency = this.currency;
			if(this.source) {
				req.src_addr = this.source;
			}
			req.dst_addr = this.target;
			fetch('/wapi/wallet/send', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						response.json().then(data => this.result = data);
					} else {
						response.text().then(data => this.error = data);
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
					<label>{{ $t('account_send_form.source_address') }}</label>
					<input type="text" v-model="source" disabled/>
				</div>
				<div class="field">
					<label>{{ $t('account_send_form.destination') }}</label>
					<select v-model="address" :disabled="!!target_">
						<option value="">{{ $t('account_send_form.address_input') }}</option>
						<option v-for="item in accounts" :key="item.account" :value="item.address">
						{{ $t('account_send_form.wallet') }} #{{item.account}} ({{item.address}})
						</option>
					</select>
				</div>
				<div class="field">
					<label>{{ $t('account_send_form.destination_address') }}</label>
					<input type="text" v-model="target" :disabled="!!address || !!target_" placeholder="mmx1..."/>
				</div>
				<div class="two fields">
					<div class="four wide field">
						<label>{{ $t('account_send_form.amount') }}</label>
						<input type="text" v-model.number="amount" placeholder="1.23" style="text-align: right"/>
					</div>
					<div class="twelve wide field">
						<label>{{ $t('account_send_form.currency') }}</label>
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
						<label>{{ $t('account_send_form.confirm') }}</label>
					</div>
				</div>
				<div @click="submit" class="ui submit primary button" :class="{disabled: !confirmed}">{{ $t('account_send_form.send') }}</div>
			</form>
		</div>
		<div class="ui message" :class="{hidden: !result}">
			{{ $t('common.transaction_has_been_sent') }}: <router-link :to="'/explore/transaction/' + result">{{result}}</router-link>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			{{ $t('common.failed_with') }}: <b>{{error}}</b>
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
		submit() {
			this.confirmed = false;
			if(this.ask_currency && !validate_address(this.ask_currency)) {
				this.error = "invalid currency address";
				return;
			}
			const req = {};
			req.index = this.index;
			req.bid = this.bid_amount;
			req.ask = this.ask_amount;
			req.bid_currency = this.bid_currency;
			req.ask_currency = this.ask_currency;
			fetch('/wapi/wallet/offer', {body: JSON.stringify(req), method: "post"})
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
		this.timer = setInterval(() => { this.update(); }, 10000);
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
						<label>{{ $t('account_offer_form.offer_amount') }}</label>
						<input type="text" v-model.number="bid_amount" placeholder="1.23" style="text-align: right"/>
					</div>
					<div class="twelve wide field">
						<label>{{ $t('account_offer_form.offer_currency') }}</label>
						<select v-model="bid_currency">
							<option v-for="item in balances" :key="item.contract" :value="item.contract">
								{{item.symbol}} <template v-if="!item.is_native"> - [{{item.contract}}]</template>
							</option>
						</select>
					</div>
				</div>
				<div class="two fields">
					<div class="four wide field">
						<label>{{ $t('account_offer_form.receive_amount') }}</label>
						<input type="text" v-model.number="ask_amount" placeholder="1.23" style="text-align: right"/>
					</div>
					<div class="two wide field">
						<label>{{ $t('account_offer_form.symbol') }}</label>
						<input type="text" v-model="ask_symbol" disabled/>
					</div>
					<div class="ten wide field">
						<label>{{ $t('account_offer_form.receive_currency_contract') }}</label>
						<input type="text" v-model="ask_currency" placeholder="mmx1..."/>
					</div>
				</div>
				<div class="inline field">
					<div class="ui toggle checkbox">
						<input type="checkbox" class="hidden" v-model="confirmed">
						<label>{{ $t('account_offer_form.confirm') }}</label>
					</div>
				</div>
				<div @click="submit" class="ui submit primary button" :class="{disabled: !confirmed}">{{ $t('account_offer_form.offer') }}</div>
			</form>
		</div>
		<div class="ui message" :class="{hidden: !result}">
			{{ $t('common.transaction_has_been_sent') }}: <router-link :to="'/explore/transaction/' + result">{{result}}</router-link>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			{{ $t('common.failed_with') }}: <b>{{error}}</b>
		</div>
		<account-offers :index="index" ref="offers"></account-offers>
		`
})

app.component('account-offers', {
	props: {
		index: Number
	},
	emits: [
		"offer-cancel"
	],
	data() {
		return {
			data: [],
			error: null,
			result: null,
			timer: null
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/offers?index=' + this.index)
				.then(response => response.json())
				.then(data => this.data = data.sort((L, R) => R.height - L.height));
		},
		cancel(id, sender) {
			const args = {};
			args.index = this.index;
			args.txid = id;
			args.address = sender;
			fetch('/wapi/wallet/revoke', {body: JSON.stringify(args), method: "post"})
				.then(response => {
					if(response.ok) {
						response.json().then(data => this.result = data);
					} else {
						response.text().then(data => this.error = data);
					}
				});
		}
	},
	watch: {
		index(value) {
			this.update();
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
				<th>{{ $t('account_offers.height') }}</th>
				<th colspan="2">{{ $t('account_offers.offer') }}</th>
				<th colspan="2">{{ $t('account_offers.receive') }}</th>
				<th>{{ $t('account_offers.address') }}</th>
				<th>{{ $t('account_offers.status') }}</th>
				<th>{{ $t('account_offers.time') }}</th>
				<th>{{ $t('account_offers.actions') }}</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data" :key="item.id">
				<td>{{item.height}}</td>
				<td class="collapsing"><b>{{item.base.input_amounts[0].value}}</b></td>
				<td>{{item.base.input_amounts[0].symbol}}</td>
				<td class="collapsing"><b>{{item.base.output_amounts[0].value}}</b></td>
				<td>{{item.base.output_amounts[0].symbol}}</td>
				<td><router-link :to="'/explore/address/' + item.id">{{item.id.substr(0, 16)}}...</router-link></td>
				<td :class="{positive: !item.base.height, negative: item.revoked}">
					<template v-if="item.base.height">
						<router-link :to="'/explore/transaction/' + item.base.id">{{item.base.input_amounts[0].accepted}}</router-link>
					</template>
					<template v-else>
						{{item.revoked ? $t('account_offers.revoked') : $t('account_offers.open') }}
					</template>
				</td>
				<td>{{new Date(item.time * 1000).toLocaleString()}}</td>
				<td>
					<template v-if="!item.revoked && !item.base.height">
						<div class="ui tiny compact button" @click="cancel(item.base.id, item.base.sender)">{{ $t('account_offers.revoke') }}</div>
					</template>
				</td>
			</tr>
			</tbody>
		</table>
		<div class="ui message" :class="{hidden: !result}">
			{{ $t('common.transaction_has_been_sent') }}: <router-link :to="'/explore/transaction/' + result">{{result}}</router-link>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			{{ $t('common.failed_with') }}: <b>{{error}}</b>
		</div>
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
					<label>{{ $t('create_contract_menu.contract_type') }}</label>
					<select v-model="type">
						<option value="locked">mmx.contract.TimeLock</option>
						<option value="virtualplot">mmx.contract.VirtualPlot</option>
					</select>
				</div>
				<div @click="submit" class="ui submit button" :class="{disabled: !type}">{{ $t('common.create') }}</div>
			</form>
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
			unlock_height: null,
			valid: false,
			confirmed: false,
			result: null,
			error: null
		}
	},
	methods: {
		check_valid() {
			this.valid = validate_address(this.owner) && this.unlock_height;
			if(!this.valid) {
				this.confirmed = false;
			}
		},
		submit() {
			this.confirmed = false;
			const contract = {};
			contract.__type = "mmx.contract.TimeLock";
			contract.owner = this.owner;
			contract.unlock_height = this.unlock_height;
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
		unlock_height(value) {
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
		<div class="ui large label">{{ $t('common.create') }}</div>
		<div class="ui large label">mmx.contract.TimeLock</div>
		<div class="ui raised segment">
			<form class="ui form" id="form">
				<div class="field">
					<label>{{ $t('create_locked_contract.owner_address') }}</label>
					<input type="text" v-model="owner" placeholder="mmx1..."/>
				</div>
				<div class="field">
					<label>{{ $t('create_locked_contract.unlock_height') }}</label>
					<input type="text" v-model.number="unlock_height"/>
				</div>
				<div class="inline field">
					<div class="ui toggle checkbox" :class="{disabled: !valid}">
						<input type="checkbox" class="hidden" v-model="confirmed">
						<label>{{ $t('common.confirm') }}</label>
					</div>
				</div>
				<div @click="submit" class="ui submit primary button" :class="{disabled: !confirmed}">{{ $t('common.deploy') }}</div>
			</form>
		</div>
		<div class="ui message" :class="{hidden: !result}">
			<template v-if="result">
				{{ $t('common.deployed_as') }}: <b>{{result}}</b>
			</template>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			{{ $t('common.failed_with') }}: <b>{{error}}</b>
		</div>
		`
})

app.component('create-virtual-plot-contract', {
	props: {
		index: Number
	},
	data() {
		return {
			farmer_key: null,
			reward_address: null,
			valid: false,
			confirmed: false,
			result: null,
			error: null
		}
	},
	methods: {
		check_valid() {
			this.valid = (this.farmer_key && this.farmer_key.length == 96)
						&& (!this.reward_address || validate_address(this.reward_address));
			if(!this.valid) {
				this.confirmed = false;
			}
		},
		submit() {
			this.confirmed = false;
			const contract = {};
			contract.__type = "mmx.contract.VirtualPlot";
			contract.farmer_key = this.farmer_key;
			if(this.reward_address) {
				contract.reward_address = this.reward_address;
			}
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
		fetch('/wapi/wallet/keys?index=' + this.index)
				.then(response => response.json())
				.then(data => this.farmer_key = data.farmer_public_key);
	},
	mounted() {
		$('.ui.checkbox').checkbox();
	},
	watch: {
		farmer_key(value) {
			this.check_valid();
		},
		reward_address(value) {
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
		<div class="ui large label">{{ $t('common.create') }}</div>
		<div class="ui large label">mmx.contract.VirtualPlot</div>
		<div class="ui raised segment">
			<form class="ui form" id="form">
				<div class="field">
					<label>{{ $t('create_virtual_plot_contract.farmer_public_key') }}</label>
					<input type="text" v-model="farmer_key"/>
				</div>
				<div class="field">
					<label>{{ $t('create_virtual_plot_contract.reward_address') }}</label>
					<input type="text" v-model="reward_address" :placeholder="$t('common.reward_address_placeholder')"/>
				</div>
				<div class="inline field">
					<div class="ui toggle checkbox" :class="{disabled: !valid}">
						<input type="checkbox" class="hidden" v-model="confirmed">
						<label>{{ $t('common.confirm') }}</label>
					</div>
				</div>
				<div @click="submit" class="ui submit primary button" :class="{disabled: !confirmed}">{{ $t('common.deploy') }}</div>
			</form>
		</div>
		<div class="ui message" :class="{hidden: !result}">
			<template v-if="result">
				{{ $t('common.deployed_as') }}: <b>{{result}}</b>
			</template>
		</div>
		<div class="ui negative message" :class="{hidden: !error}">
			{{ $t('common.failed_with') }}: <b>{{error}}</b>
		</div>
		`
})
