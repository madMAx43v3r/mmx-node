
function validate_address(address) {
	return address && address.length == 62 && address.startsWith("mmx1");
}

const Wallet = {
	template: '<wallet-summary></wallet-summary>'
}
const WalletCreate = {
	template: '<create-wallet></create-wallet>'
}
const Account = {
	props: {
		index: Number
	},
	template: `
		<v-card>
			<v-card-text>
				<account-header :index="index"></account-header>
				<account-menu :index="index" class="my-2"></account-menu>
				<router-view :index="index"></router-view>
			</v-card-text>
		</v-card>
	`
}
const AccountHome = {
	props: {
		index: Number
	},
	template: `
		<div>
			<account-balance :index="index" class="my-2"></account-balance>
			<account-history :index="index" :limit="50"></account-history>
		</div>
	`
}
const AccountNFTs = {
	props: {
		index: Number
	},
	template: `
		<nft-table :index="index"></nft-table>
	`
}
const AccountContracts = {
	props: {
		index: Number
	},
	template: `
		<div>
			<create-contract-menu :index="index"></create-contract-menu>
			<account-contracts :index="index"></account-contracts>
		</div>
	`
}
const AccountAddresses = {
	props: {
		index: Number
	},
	template: `
		<account-addresses :index="index" :limit="1000"></account-addresses>
	`
}
const AccountSend = {
	props: {
		index: Number
	},
	template: `
		<account-send-form :index="index" :target_="$route.params.target"></account-send-form>
	`
}
const AccountSendFrom = {
	props: {
		index: Number
	},
	template: `
		<account-send-form :index="index" :source_="$route.params.source"></account-send-form>
	`
}
const AccountSplit = {
	props: {
		index: Number
	},
	template: `
		<account-split-form :index="index"></account-split-form>
	`
}
const AccountOffer = {
	props: {
		index: Number
	},
	template: `
		<account-offer-form :index="index"></account-offer-form>
	`
}
const AccountHistory = {
	props: {
		index: Number
	},
	template: `
		<account-history :index="index" :limit="200"></account-history>
	`
}
const AccountLog = {
	props: {
		index: Number
	},
	template: `
		<account-tx-history :index="index" :limit="200"></account-tx-history>
	`
}
const AccountDetails = {
	props: {
		index: Number
	},
	template: `
		<account-details :index="index"></account-details>
	`
}
const AccountOptions = {
	props: {
		index: Number
	},
	template: `
		<div>
			<account-actions :index="index"></account-actions>
			<create-account :index="index" class="my-2"></create-account>
		</div>
	`
}
const AccountCreateLocked = {
	props: {
		index: Number
	},
	template: `
		<create-locked-contract :index="index"></create-locked-contract>
	`
}
const AccountCreateVirtualPlot = {
	props: {
		index: Number
	},
	template: `
		<create-virtual-plot-contract :index="index"></create-virtual-plot-contract>
	`
}

const Market = {
	props: {
		wallet: Number,
		bid: null,
		ask: null,
	},
	template: `
		<div>
			<market-menu :wallet_="wallet" :bid_="bid" :ask_="ask" :page="$route.meta.page"></market-menu>
			<router-view :key="$route.path" :wallet="wallet" :bid="bid" :ask="ask"></router-view>
		</div>
		`
}
const MarketOffers = {
	props: {
		wallet: Number,
		bid: null,
		ask: null,
	},
	template: `
		<market-offers ref="orders" :wallet="wallet" :bid="bid" :ask="ask" :limit="200"></market-offers>
		`
}

const Exchange = {
	props: {
		wallet: Number,
		server: String,
		bid: String,
		ask: String,
	},
	data() {
		return {
			bid_symbol: null,
			ask_symbol: null
		}
	},
	methods: {
		update_bid_symbol(value) {
			this.bid_symbol = value;
		},
		update_ask_symbol(value) {
			this.ask_symbol = value;
		}
	},
	template: `
		<div>
			<exchange-menu @update-bid-symbol="update_bid_symbol" @update-ask-symbol="update_ask_symbol"
				:wallet_="wallet" :server_="server" :bid_="bid" :ask_="ask" :page="$route.meta.page">
			</exchange-menu>
			<router-view :key="$route.path" :wallet="wallet" :server="server" :bid="bid" :ask="ask" :bid_symbol="bid_symbol" :ask_symbol="ask_symbol"></router-view>
		</div>
		`
}
const ExchangeMarket = {
	props: {
		wallet: Number,
		server: String,
		bid: String,
		ask: String,
		bid_symbol: String,
		ask_symbol: String
	},
	methods: {
		update() {
			this.$refs.orders.update();
		}
	},
	template: `
		<div>
			<div class="ui two column grid">
				<div class="column">
					<exchange-trade-form @trade-executed="update"
						:wallet="wallet" :server="server"
						:bid_symbol="ask_symbol" :ask_symbol="bid_symbol"
						:bid_currency="ask" :ask_currency="bid" :action="'Buy ' + bid_symbol">
					</exchange-trade-form>
				</div>
				<div class="column">
					<exchange-trade-form @trade-executed="update"
						:wallet="wallet" :server="server"
						:bid_symbol="bid_symbol" :ask_symbol="ask_symbol"
						:bid_currency="bid" :ask_currency="ask" :action="'Sell ' + bid_symbol">
					</exchange-trade-form>
				</div>
			</div>
			<exchange-orders ref="orders" :server="server" :bid="bid" :ask="ask" :flip="false" :limit="100"></exchange-orders>
		</div>
		`
}

const ExchangeTrades = {
	props: {
		server: String,
		bid: String,
		ask: String,
		bid_symbol: String,
		ask_symbol: String
	},
	template: `
		<exchange-trades :server="server" :bid="bid" :ask="ask" :limit="200"></exchange-trades>
		`
}
const ExchangeHistory = {
	props: {
		server: String,
		bid: String,
		ask: String,
		bid_symbol: String,
		ask_symbol: String
	},
	template: `
		<exchange-history :bid="bid" :ask="ask" :limit="200"></exchange-history>
		`
}
const ExchangeOffers = {
	props: {
		wallet: Number,
		server: String,
		bid: String,
		ask: String,
		bid_symbol: String,
		ask_symbol: String
	},
	methods: {
		update_created() {
			this.$refs.orders.update();
			this.$refs.offers.update();
		},
		update_cancel() {
			this.$refs.orders.update();
			this.$refs.bid_form.update();
			this.$refs.ask_form.update();
		}
	},
	template: `
		<div>
			<div class="ui two column grid">
				<div class="column">
					<exchange-offer-form ref="bid_form" @offer-created="update_created"
						:wallet="wallet" :server="server"
						:bid_symbol="bid_symbol" :ask_symbol="ask_symbol" :flip="false"
						:bid_currency="bid" :ask_currency="ask">
					</exchange-offer-form>
				</div>
				<div class="column">
					<exchange-offer-form ref="ask_form" @offer-created="update_created"
						:wallet="wallet" :server="server"
						:bid_symbol="ask_symbol" :ask_symbol="bid_symbol" :flip="true"
						:bid_currency="ask" :ask_currency="bid">
					</exchange-offer-form>
				</div>
			</div>
			<exchange-orders ref="orders" :server="server" :bid="bid" :ask="ask" :flip="true" :limit="5"></exchange-orders>
			<account-offers ref="offers" @offer-cancel="update_cancel" :index="wallet" :bid="bid" :ask="ask"></account-offers>
		</div>
		`
}

const Explore = {
	template: `
	<v-card class="my-2">
		<v-card-text>
			<explore-menu></explore-menu>
			<router-view></router-view>
		</v-card-text>
	</v-card>
	`
}
const ExploreBlocks = {
	template: `
		<explore-blocks :limit="100"></explore-blocks>
	`
}
const ExploreTransactions = {
	template: `
		<explore-transactions :limit="100"></explore-transactions>
	`
}
const ExploreBlock = {
	template: `
		<block-view :hash="$route.params.hash" :height="parseInt($route.params.height)"></block-view>
	`
}
const ExploreAddress = {
	template: `
		<address-view :address="$route.params.address"></address-view>
	`
}
const ExploreTransaction = {
	template: `
		<transaction-view :id="$route.params.id"></transaction-view>
	`
}

const NodeView = {
	template: `
		<div>
			<node-info></node-info>
			<farmer-info></farmer-info>
			<node-menu class="mt-4"></node-menu>
			<router-view class="mt-2"></router-view>
		</div>
	`
}
const NodeBlocks = {
	template: `
		<explore-blocks :limit="20"></explore-blocks>
	`
}
const NodeLog = {
	template: `
		<node-log></node-log>
	`
}
const NodePeers = {
	template: `
		<node-peers></node-peers>
	`
}
const NodeNetspace = {
	template: `
		<netspace-graph :limit="1000" :step="90"></netspace-graph>
	`
}
const NodeVDFSpeed = {
	template: `
		<vdf-speed-graph :limit="1000" :step="90"></vdf-speed-graph>
	`
}
const NodeBlockReward = {
	template: `
		<block-reward-graph :limit="1000" :step="90"></block-reward-graph>
	`
}

const Settings = {
	template: `
		<div>
			<node-settings></node-settings>
			<wallet-settings></wallet-settings>
		</div>
	`
}

const Login = {
	data() {
		return {
			passwd: null,
			error: null
		}
	},
	methods: {
		submit() {
			this.error = null;
			fetch('/server/login?user=mmx-admin&passwd_plain=' + this.passwd)
				.then(response => {
					if(response.ok) {
						this.$router.push("/");
					} else {
						this.error = "Login failed!";
					}
				});
		}
	},
	template: `
		<div>
			<v-card>
				<v-card-text>
					<!-- TODO add validation rulues -->
					<v-text-field
						v-model="passwd"
						:label="$t('login.password_label')"
						required
						type="password"/>
					<v-btn outlined color="primary" @click="submit">{{ $t('login.login') }}</v-btn>
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
				{{ error }}
			</v-alert>
		</div>
		`
}

Vue.component('main-menu', {
	methods: {
		data() {
			return {
				timer: null
			}
		},		
		update() {
			fetch('/server/session')
				.then(response => response.json())
				.then(data => {
					if(!data.user) {
						if(this.$router.currentRoute.path != "/login") {
							this.$router.push("/login");
						}
					}
				});
		},
		logout() {
			fetch('/server/logout')
				.then(response => {
					if(response.ok) {
						this.$router.push("/login");
					}
				});
		}
	},
	created() {
		if(!this.$isWinGUI) 
		{
			this.update();
			this.timer = setInterval(() => { this.update(); }, 5000);
		}
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<v-tabs>
			<status/>
			<v-tab to="/node">{{ $t('main_menu.node') }}</v-tab>
			<v-tab to="/wallet">{{ $t('main_menu.wallet') }}</v-tab>
			<v-tab to="/explore">{{ $t('main_menu.explore') }}</v-tab>
			<v-tab to="/market">{{ $t('main_menu.market') }}</v-tab>
			<v-spacer></v-spacer>
			<v-tab to="/settings">{{ $t('main_menu.settings') }}</v-tab>
			<template v-if="!$route.meta.is_login && !$isWinGUI">
				<v-tab @click="logout">{{ $t('main_menu.logout') }}</v-tab>
			</template>
		</v-tabs>
		`
})

const AppStatus = {
	DisconnectedFromNode: "DisconnectedFromNode",
	Connecting: "Connecting",
	Syncing: "Syncing",
	Synced: "Synced"
}

Vue.component('status', {
	data() {
		return {
			timer: null,
			session_fails: 99,
			peer_fails: 99,
			synced_fails: 99,
		}
	},
	methods: {
		async update() {

			if(!this.connectedToNetwork) {
				await fetch('/server/session')
					.then( () => this.session_fails = 0 )
					.catch( () => this.session_fails++ );
			}

			if(this.connectedToNode && !this.synced) {
				await fetch('/api/router/get_peer_info')
					.then( response => response.json() )
					.then( data => {
						if(data.peers && data.peers.length > 0) {
							this.peer_fails = 0
						} else {
							this.peer_fails++
						}
					})
					.catch( () => this.peer_fails++ );
			}

			if(this.connectedToNetwork) {
				await fetch('/wapi/node/info')
					.then( response => response.json() )
					.then( data => {
						this.$root.nodeInfo = data
						if(data.is_synced) {
							this.synced_fails = 0
						} else {
							this.synced_fails++
						}
					})
					.catch( () => this.synced_fails++ );				
			}
			
			// console.log('--------------------------------');
			// console.log('session_fails', this.session_fails);
			// console.log('connectedToNode', this.connectedToNode);

			// console.log('peer_fails', this.peer_fails);
			// console.log('connectedToNetwork', this.connectedToNetwork);

			// console.log('synced_fails', this.synced_fails);
			// console.log('synced', this.synced);			
		},
	},
	computed: {
		connectedToNode() {
			return this.session_fails < 1;
		},
		connectedToNetwork() {
			return this.connectedToNode && this.peer_fails < 1;
		},
		synced() {
			return this.connectedToNetwork && this.synced_fails == 0;
		},
		status() {
			let result = AppStatus.DisconnectedFromNode
			if(this.connectedToNode) {
				result = AppStatus.Connecting;
				if(this.connectedToNetwork) {
					result = AppStatus.Syncing;
					if(this.synced) {
						result = AppStatus.Synced;
					}
				}
			}
			return result;
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 5000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<div class="d-flex align-center px-5">
			<v-icon color="red"    v-if="status == AppStatus.Connecting">mdi-connection</v-icon>
			<v-icon color="yellow" v-else-if="status == AppStatus.Syncing">mdi-sync</v-icon>
			<v-icon color="green"  v-else-if="status == AppStatus.Synced">mdi-cloud-check</v-icon>
			<v-icon color="red"    v-else>mdi-emoticon-dead</v-icon>
		</div>
		`
})

Vue.component('app', {
	computed: {
		colClass(){
			var result = '';
			if(!this.$isWinGUI) {
				result = 'cols-12 col-xl-8 offset-xl-2';
			}
			return result;
		},
		fluid() {
			return this.$isWinGUI || !(this.$vuetify.breakpoint.xl || this.$vuetify.breakpoint.lg);
		}
	},
	template: `
		<v-app>
			<v-app-bar app dense>
				<v-container :fluid="fluid" class="main_container">
					<v-row>
						<v-col :class="colClass">
							<main-menu></main-menu>
						</v-col>
					</v-row>
				</v-container>
			</v-app-bar>
			<v-main>
				<v-container :fluid="fluid" class="main_container">
					<v-row>
						<v-col :class="colClass">
							<router-view></router-view>
						</v-col>
					</v-row>
				</v-container>                
			</v-main>
			<!--v-footer app>
			</v-footer-->
		</v-app>
		`
})

