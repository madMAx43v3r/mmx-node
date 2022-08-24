
Vue.component('wallet-summary', {
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
		<div>
			<v-progress-linear :active="loading" indeterminate absolute top></v-progress-linear>

			<div v-for="item in data" :key="item[0]">
				<account-summary :index="item[0]" :account="item[1]"></account-summary>
			</div>

			<v-btn to="/wallet/create" outlined color="primary">{{ $t('wallet_summary.new_wallet') }}</v-btn>
		</div>
		`
})

Vue.component('account-menu', {
	props: {
		index: Number
	},
	template: `
		<v-btn-toggle>
			<v-btn :to="'/wallet/account/' + index" exact>{{ $t('account_menu.balance') }}</v-btn>
			<v-btn :to="'/wallet/account/' + index + '/nfts'">{{ $t('account_menu.nfts') }}</v-btn>
			<v-btn :to="'/wallet/account/' + index + '/contracts'">{{ $t('account_menu.contracts') }}</v-btn>
			<v-btn :to="'/wallet/account/' + index + '/addresses'">{{ $t('account_menu.addresses') }}</v-btn>
			<v-btn :to="'/wallet/account/' + index + '/send'">{{ $t('account_menu.send') }}</v-btn>
			<v-btn :to="'/wallet/account/' + index + '/offer'">{{ $t('account_menu.offer') }}</v-btn>
			<v-btn :to="'/wallet/account/' + index + '/history'">{{ $t('account_menu.history') }}</v-btn>
			<v-btn :to="'/wallet/account/' + index + '/log'">{{ $t('account_menu.log') }}</v-btn>
			<v-btn :to="'/wallet/account/' + index + '/details'">{{ $t('account_menu.details') }}</v-btn>
			<v-btn :to="'/wallet/account/' + index + '/options'"><v-icon>mdi-cog</v-icon></v-btn>
		</v-btn-toggle>
		`
})

Vue.component('account-header', {
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
		<div>
			<v-chip label>{{ $t('account_header.wallet') }} #{{index}}</v-chip>
			<v-chip label>{{ address }}</v-chip>
		</div>
		`
})

Vue.component('account-summary', {
	props: {
		index: Number,
		account: Object
	},
	template: `
		<v-card class="my-2">
			<v-card-text>
				<account-header :index="index" :account="account"></account-header>
				<account-menu :index="index" class="my-2"></account-menu>				
				<account-balance :index="index"></account-balance>
			</v-card-text>
		</v-card>
		`
})

Vue.component('account-balance', {
	props: {
		index: Number
	},
	data() {
		return {
			data: [],
			loaded: false,
			timer: null,
			headers: [
				{ text: this.$t('account_balance.balance'), value: 'total' },
				{ text: this.$t('account_balance.reserved'), value: 'reserved' },
				{ text: this.$t('account_balance.spendable'), value: 'spendable' },
				{ text: this.$t('account_balance.token'), value: 'token' },
				{ text: this.$t('account_balance.contract'), value: 'contract' },

			]
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/wallet/balance?index=' + this.index)
				.then(response => response.json())
				.then(data => {
					this.loaded = true;
					this.data = data.balances;
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 10000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<v-data-table
			:headers="headers"
			:items="data"
			:loading="!loaded"
			hide-default-footer
			disable-sort
			disable-pagination
			class="elevation-2"
		>
			<template v-slot:item.contract="{ item }">
				<router-link :to="'/explore/address/' + item.contract">{{item.is_native ? '' : item.contract}}</router-link>
			</template>

			<template v-slot:item.spendable="{ item }">
				<b>{{item.spendable}}</b>
			</template>

			<template v-slot:item.token="{ item }">
				<div :class="{'text--disabled': !item.is_validated}">{{item.symbol}}</div>
			</template>

		</v-data-table>
		`
})

Vue.component('balance-table', {
	props: {
		address: String,
		show_empty: Boolean
	},
	data() {
		return {
			data: [],
			loading: false,
			loaded: false,
			timer: null,
			headers: [
				{ text: this.$t('balance_table.balance'), value: 'total'},
				{ text: this.$t('balance_table.locked'), value: 'locked'},
				{ text: this.$t('balance_table.spendable'), value: 'spendable'},
				{ text: this.$t('balance_table.token'), value: 'symbol'},
				{ text: this.$t('balance_table.contract'), value: 'contract'},
			]
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/balance?id=' + this.address)
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.loaded = true;
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
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<v-data-table
			:headers="headers"
			:items="data"
			:loading="!loaded"
			hide-default-footer
			disable-sort
			disable-pagination
			class="elevation-2"
			v-if="!loaded || loaded && (data.length || show_empty)"
		>
			<template v-slot:item.spendable="{ item }">
				<b>{{item.spendable}}</b>
			</template>

			<template v-slot:item.contract="{ item }">
				<router-link :to="'/explore/address/' + item.contract">{{item.is_native ? '' : item.contract}}</router-link>
			</template>
		</v-data-table>

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

Vue.component('nft-table', {
	props: {
		index: Number
	},
	data() {
		return {
			nfts: [],
			headers: [
				{ text: 'NFT', value: 'item' },
			]
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
		<v-data-table
			:headers="headers"
			:items="nfts"
			hide-default-footer
			disable-sort
			disable-pagination
			class="elevation-2"
		>
			<template v-slot:item.item="{ item }">
				<router-link :to="'/explore/address/' + item">{{item}}</router-link>
			</template>
		</v-data-table>
		`
})

Vue.component('account-history', {
	props: {
		index: Number,
		limit: Number
	},
	data() {
		return {
			data: [],
			loading: false,
			loaded: false,
			timer: null,
			headers: [
				{ text: this.$t('account_history.height'), value: 'height' },
				{ text: this.$t('account_history.type'), value: 'type' },
				{ text: this.$t('account_history.amount'), value: 'value' },
				{ text: this.$t('account_history.token'), value: 'token' },
				{ text: this.$t('account_history.address'), value: 'address' },
				{ text: this.$t('account_history.link'), value: 'link' },
				{ text: this.$t('account_history.time'), value: 'time' },
			]
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/wallet/history?limit=' + this.limit + '&index=' + this.index)
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.loaded = true;
					this.data = data;
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
		<v-data-table
			:headers="headers"
			:items="data"
			:loading="!loaded"
			hide-default-footer
			disable-sort
			disable-pagination
			class="elevation-2"
		>
			<template v-slot:item.height="{ item }">
				<router-link :to="'/explore/block/height/' + item.height">{{item.height}}</router-link>
			</template>	

			<template v-slot:item.value="{ item }">
				<b>{{item.value}}</b>
			</template>

			<template v-slot:item.token="{ item }">
				<template v-if="item.is_native">
					{{item.symbol}}
				</template>
				<template v-else>
					<router-link :to="'/explore/address/' + item.contract">{{item.is_nft ? "[NFT]" : item.symbol}}{{item.is_validated ? "" : "?"}}</router-link>
				</template>
			</template>

			<template v-slot:item.address="{ item }">
				<router-link :to="'/explore/address/' + item.address">{{item.address}}</router-link>
			</template>
			
			<template v-slot:item.link="{ item }">
				<router-link :to="'/explore/transaction/' + item.txid">TX</router-link>
			</template>

			<template v-slot:item.time="{ item }">
				{{new Date(item.time * 1000).toLocaleString()}}
			</template>

		</v-data-table>
		`
})

Vue.component('account-tx-history', {
	props: {
		index: Number,
		limit: Number
	},
	data() {
		return {
			data: [],
			loading: false,
			loaded: false,
			timer: null,
			headers: [
				{ text: this.$t('account_tx_history.height'), value: 'height' },
				{ text: this.$t('account_tx_history.confirmed'), value: 'confirmed' },
				{ text: this.$t('account_tx_history.transaction_id'), value: 'transaction_id' },
				{ text: this.$t('account_tx_history.time'), value: 'time' },
			]			
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/wallet/tx_history?limit=' + this.limit + '&index=' + this.index)
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.loaded = true;
					this.data = data;
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 10000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<v-data-table
			:headers="headers"
			:items="data"
			:loading="!loaded"
			hide-default-footer
			disable-sort
			disable-pagination
			class="elevation-2"
		>

			<template v-slot:item.height="{ item }">
				<template v-if="item.height">
					<router-link :to="'/explore/block/height/' + item.height">{{item.height}}</router-link>
				</template>
				<template v-else>
					<i>{{ $t('account_tx_history.pending') }}</i>
				</template>
			</template>

			<template v-slot:item.transaction_id="{ item }">
				<router-link :to="'/explore/transaction/' + item.id">{{item.id}}</router-link>
			</template>

			<template v-slot:item.confirmed="{ item }">
				{{item.confirm ? item.confirm > 1000 ? "> 1000" : item.confirm : 0}}
			</template>								

			<template v-slot:item.time="{ item }">
				{{new Date(item.time).toLocaleString()}}
			</template>

		</v-data-table>
		`
})

Vue.component('account-contract-summary', {
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
		<v-card class="my-2">
			<v-card-text>			
				<v-chip label>{{contract.__type}}</v-chip>
				<v-chip label>{{address}}</v-chip>
				<object-table :data="contract" class="my-2"></object-table>
				<balance-table :address="address"></balance-table>
				<div class="mt-4">
					<v-btn outlined @click="deposit">{{ $t('account_contract_summary.deposit') }}</v-btn>
					<v-btn outlined @click="withdraw">{{ $t('account_contract_summary.withdraw') }}</v-btn>
				</div>
			</v-card-text>
		</div>
		`
})

Vue.component('account-contracts', {
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
		<v-card outlined color="transparent">
			<v-progress-linear  v-if="!data && loading" indeterminate absolute top></v-progress-linear>
			<account-contract-summary v-if="data" v-for="item in data" :key="item[0]" :index="index" :address="item[0]" :contract="item[1]"></account-contract-summary>
		</v-card>
		`
})

Vue.component('account-addresses', {
	props: {
		index: Number,
		limit: Number
	},
	data() {
		return {
			data: [],
			headers: [
				{ text: this.$t('account_addresses.index'), value: 'index' },
				{ text: this.$t('account_addresses.address'), value: 'address' },
				{ text: this.$t('account_addresses.n_recv'), value: 'n_recv' },
				{ text: this.$t('account_addresses.n_spend'), value: 'n_spend' },
				{ text: this.$t('account_addresses.last_recv'), value: 'last_recv' },
				{ text: this.$t('account_addresses.last_spend'), value: 'last_spend' },
			]
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
		<v-data-table
			:headers="headers"
			:items="data"
			hide-default-footer
			disable-sort
			disable-pagination
			class="elevation-2"
		>
			<template v-slot:item.index="{ item, index }">
				{{ index }}
			</template>	

			<template v-slot:item.address="{ item }">
				<router-link :to="'/explore/address/' + item">{{item}}</router-link>
			</template>				
		
		</v-data-table>
		`
})

Vue.component('account-details', {
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
		<div>
			<object-table :data="account"></object-table>
			<object-table :data="keys" class="my-2"></object-table>

			<v-btn v-if="$isWinGUI && this.keys" @click="copyKeysToPlotter" color="primary">{{ $t('account_details.copy_keys_to_plotter') }}</v-btn>
		</div>
		`
})

Vue.component('account-actions', {
	props: {
		index: Number
	},
	data() {
		return {
			seed: null,
			info: null,
			error: null,
			dialog: false
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
					this.dialog = true;
				});
		}
	},
	template: `
		<div>
			<v-card>
				<v-card-text>
					<v-btn outlined @click="reset_cache">{{ $t('account_actions.reset_cache') }}</v-btn>

					<v-dialog v-model="dialog"
						transition="dialog-top-transition"
						max-width="700"
					>
						<template v-slot:activator="{ on, attrs }">
							<v-btn outlined @click="show_seed">{{ $t('account_actions.show_seed') }}</v-btn>
						</template>
						<template v-slot:default="dialog">
							<v-card>
								<v-toolbar
								color="primary"
								dark
								>
								</v-toolbar>
								<v-card-text>
									<v-container>
										<object-table :data="seed"></object-table>
									</v-container>
								</v-card-text>
								<v-card-actions class="justify-end">
									<v-btn
										text
										@click="dialog.value = false"
									>Close</v-btn>
								</v-card-actions>
							</v-card>
						</template>
					</v-dialog>

				</v-card-text>
			</v-card>

			<v-alert
				border="left"
				colored-border
				type="info"
				dismissible
				elevation="2"
				v-if="info"
				class="my-2"
			>
				{{info}}
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
	
		</div>
		`
})

Vue.component('create-account', {
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
		<div>
			<v-card>
				<v-card-text>
					<v-row >
						<v-col>
							<v-text-field type="text" :label="$t('create_account.account_index')" v-model.number="offset" class="text-align-right"/>
						</v-col>
						<v-col cols="6">							
							<v-text-field type="text" :label="$t('create_account.account_name')" v-model="name"/>
						</v-col>
						<v-col>
							<v-text-field type="text" :label="$t('create_account.number_of_addresses')" v-model.number="num_addresses"/>						
						</v-col>
					</v-row>
					<v-btn outlined @click="submit">{{ $t('create_account.create_account') }}</v-btn>
				</v-card-text>
			</v-card>

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

		</div>

		`
})

Vue.component('create-wallet', {
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
	template: `
		<div>
			<v-card>
				<v-card-text>

					<v-row>
						<v-col>
							<v-text-field
								v-model="name"
								:label="$t('create_wallet.account_name')">
							</v-text-field>
						</v-col>
						<v-col cols=4>

							<v-text-field
								v-model.number="num_addresses"
								:label="$t('create_wallet.number_of_addresses')"
								class="text-align-right">
							</v-text-field>
						</v-col>
					</v-row>

					<v-checkbox
						v-model="with_seed"
						:label="$t('create_wallet.use_custom_seed')">
					</v-checkbox>

					<v-text-field
						v-model="seed"
						:label="$t('create_wallet.seed_hash')"
						:placeholder="$t('create_wallet.placeholder')" 
						:disabled="!with_seed">
					</v-text-field>

					<v-btn @click="submit" outlined color="primary">{{ $t('create_wallet.create_wallet') }}</v-btn>

				</v-card-text>

			</v-card>
							
			<v-alert
				border="left"
				colored-border
				type="error"
				v-if="error"
				elevation="2"
				class="my-2"
			>
				{{ $t('common.failed_with') }}: <b>{{error}}</b>
			</v-alert>

		</div>

		`
})

Vue.component('account-send-form', {
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
	computed: {
		selectAccounts() {
			var result = [];
			
			result.push({ text: this.$t('account_send_form.address_input'), value: ""});

			this.accounts.map(item => {
				result.push(
					{
						text: `${this.$t('account_send_form.wallet') } #${item.account} (${item.address})`,
						value: item.address
					}
				);
			});

			return result;
		}
	},
	template: `
	<div>
		<balance-table :address="source" :show_empty="true" v-if="source" ref="balance"></balance-table>
		<account-balance :index="index" v-if="!source" ref="balance"></account-balance>

		<v-card class="my-2">
			<v-card-text>
				<v-text-field 
					v-if="!!source"
					v-model="source" 
					:label="$t('account_send_form.source_address')" disabled>
				</v-text-field>

				<v-select 
					v-model="address"
					:label="$t('account_send_form.destination')"
					:items="selectAccounts"
					item-text="text"
					item-value="value"
					:disabled="!!target_">
				</v-select>

				<v-text-field 
					v-model="target"
					:label="$t('account_send_form.destination_address')"
					:disabled="!!address || !!target_" placeholder="mmx1...">
				</v-text-field>

				<v-row>
					<v-col cols="3">
						<v-text-field class="text-align-right"
						:label="$t('account_send_form.amount')"
						v-model.number="amount" placeholder="1.23"
						></v-text-field>
					</v-col>
					<v-col>
						<v-select
							v-model="currency"
							:label="$t('account_send_form.currency')"
							:items="balances"
							item-text="contract"
							item-value="contract">

							<template v-for="slotName in ['item', 'selection']" v-slot:[slotName]="{ item }">
								{{item.symbol}} <template v-if="!item.is_native"> - [{{item.contract}}]</template>
							</template>

						</v-select>
					</v-col>
				</v-row>

				<v-switch v-model="confirmed" :label="$t('account_offer_form.confirm')"></v-switch>
				<v-btn @click="submit" outlined color="primary" :disabled="!confirmed">{{ $t('account_send_form.send') }}</v-btn>

			</v-card-text>
		</v-card>

		<v-alert
			border="left"
			colored-border
			type="info"
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

		<account-tx-history :index="index" :limit="10" ref="history"></account-tx-history>
	</div>
		`
})

Vue.component('account-offer-form', {
	props: {
		index: Number
	},
	data() {
		return {
			tokens: [],
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
			fetch('/wapi/wallet/tokens')
				.then(response => response.json())
				.then(data => this.tokens = data);
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
	beforeDestroy() {
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
			if(value && value !== 'mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev') {
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
		<div>
			<account-balance :index="index" ref="balance"></account-balance>
			<v-card class="my-2">
				<v-card-text>

					<v-row>
						<v-col cols="3">
							<v-text-field class="text-align-right"
								:label="$t('account_offer_form.offer_amount')"
								placeholder="1.23"
								v-model.number="bid_amount"	
							></v-text-field>
						</v-col>
						<v-col>
							<v-select
								v-model="bid_currency"
								:label="$t('account_offer_form.offer_currency')"
								:items="balances" 
								item-text="contract"
								item-value="contract">

								<template v-for="slotName in ['item', 'selection']" v-slot:[slotName]="{ item }">
									{{item.symbol}} <template v-if="!item.is_native"> - [{{item.contract}}]</template>
								</template>

							</v-select>
						</v-col>
					</v-row>

					<v-row>
						<v-col cols="3">
							<v-text-field class="text-align-right"
								:label="$t('account_offer_form.receive_amount')"
								placeholder="1.23"
								v-model.number="ask_amount"	
							></v-text-field>
						</v-col>
						<v-col cols="1">
							<v-text-field
								:label="$t('account_offer_form.symbol')"
								placeholder="1.23"
								v-model="ask_symbol"
								disabled
							></v-text-field>
						</v-col>
						<v-col>
							<v-select
								v-model="ask_currency"
								:label="$t('account_offer_form.receive_currency_contract')"
								:items="tokens" 
								item-text="currency"
								item-value="currency">

								<template v-for="slotName in ['item', 'selection']" v-slot:[slotName]="{ item }">
									{{item.symbol}}<template v-if="item.currency != 'mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev'"> - [{{item.currency}}]</template>
								</template>

							</v-select>
						</v-col>
					</v-row>

					<v-switch v-model="confirmed" :label="$t('account_offer_form.confirm')"></v-switch>
					<v-btn @click="submit" outlined color="primary" :disabled="!confirmed">{{ $t('account_offer_form.offer') }}</v-btn>

				</v-card-text>
			</v-card>

			<v-alert
				border="left"
				colored-border
				type="info"
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

			<account-offers :index="index" ref="offers"></account-offers>

		</div>
		`
})

Vue.component('account-offers', {
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
	beforeDestroy() {
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
						<router-link :to="'/explore/transaction/' + item.base.id">{{ $t('account_offers.accepted') }}</router-link>
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

Vue.component('create-contract-menu', {
	props: {
		index: Number
	},
	data() {
		return {
			type: null,
			types: [
				{ value: "locked", text: "mmx.contract.TimeLock" },
				{ value: "virtualplot", text: "mmx.contract.VirtualPlot" },
			]
		}
	},
	methods: {
		submit() {
			this.$router.push("/wallet/account/" + this.index + "/create/" + this.type);
		}
	},
	template: `
		<v-card>
			<v-card-text>
				<v-select v-model="type" :items="types" :label="$t('create_contract_menu.contract_type')">
				</v-select>
				<v-btn outlined @click="submit" :disabled="!type">{{ $t('common.create') }}</v-btn>
			</v-card-text>
		</v-card>
		`
})

Vue.component('create-locked-contract', {
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
		<div>
			<v-chip label>{{ $t('common.create') }}</v-chip>
			<v-chip label>mmx.contract.TimeLock</v-chip>

			<v-card class="my-2">
				<v-card-text>
					<v-text-field 
						:label="$t('create_locked_contract.owner_address')"
						v-model="owner" 
						placeholder="mmx1...">
					</v-text-field>

					<v-text-field 
						:label="$t('create_locked_contract.unlock_height')"
						v-model.number="unlock_height">
					</v-text-field>

					<v-switch 
						v-model="confirmed"
						:disabled="!valid"
						:label="$t('common.confirm')">
					</v-switch>

					<v-btn @click="submit" outlined color="primary" :disabled="!valid">{{ $t('common.deploy') }}</v-btn>

				</v-card-text>
			</v-card>

			<v-alert
				border="left"
				colored-border
				type="info"
				v-if="result"
				elevation="2"
				class="my-2"
			>
				{{ $t('common.deployed_as') }}: <b>{{result}}</b>
			</v-alert>

			<v-alert
				border="left"
				colored-border
				type="error"
				v-if="error"
				elevation="2"
				class="my-2"
			>
				{{ $t('common.failed_with') }}: <b>{{error}}</b>
			</v-alert>

		</div>
		`
})

Vue.component('create-virtual-plot-contract', {
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
		<div>
			<v-chip label>{{ $t('common.create') }}</v-chip>
			<v-chip label>mmx.contract.VirtualPlot</v-chip>

			<v-card class="my-2">
				<v-card-text>
					<v-text-field 
						:label="$t('create_virtual_plot_contract.farmer_public_key')"
						v-model="farmer_key">
					</v-text-field>

					<v-text-field 
						:label="$t('create_virtual_plot_contract.reward_address')"
						v-model="reward_address" 
						:placeholder="$t('common.reward_address_placeholder')">
					</v-text-field>

					<v-switch 
						v-model="confirmed"
						:disabled="!valid"
						:label="$t('common.confirm')">
					</v-switch>

					<v-btn @click="submit" outlined color="primary" :disabled="!valid">{{ $t('common.deploy') }}</v-btn>

				</v-card-text>
			</v-card>

			<v-alert
				border="left"
				colored-border
				type="info"
				v-if="result"
				elevation="2"
				class="my-2"
			>
				{{ $t('common.deployed_as') }}: <b>{{result}}</b>
			</v-alert>

			<v-alert
				border="left"
				colored-border
				type="error"
				v-if="error"
				elevation="2"
				class="my-2"
			>
				{{ $t('common.failed_with') }}: <b>{{error}}</b>
			</v-alert>

		</div>
		`
})
