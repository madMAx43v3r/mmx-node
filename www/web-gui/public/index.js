
function validate_address(address) {
	return address && address.length == 62 && address.startsWith("mmx1");
}

const MMX_ADDR = "mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev";

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
		<account-history-form :index="index" :limit="200"></account-history-form>
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
		<market-offers :wallet="wallet" :bid="bid" :ask="ask" :limit="200"></market-offers>
		`
}

const MarketHistory = {
	props: {
		bid: null,
		ask: null,
	},
	template: `
		<market-history :bid="bid" :ask="ask" :limit="200"></market-history>
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
const ExploreFarmers = {
	template: `
		<explore-farmers :limit="100"></explore-farmers>
	`
}
const ExploreBlock = {
	template: `
		<block-view :hash="$route.params.hash" :height="parseInt($route.params.height)"></block-view>
	`
}
const ExploreFarmer = {
	template: `
		<farmer-view :farmer_key="$route.params.id" limit="100"></farmer-view>
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

const Farmer = {
	template: `
		<div>
			<farmer-info></farmer-info>
			<farmer-menu class="mt-4"></farmer-menu>
			<router-view class="mt-2"></router-view>
		</div>
	`
}
const FarmerBlocks = {
	template: `
		<farmer-blocks limit="200"></farmer-blocks>
	`
}
const FarmerProofs = {
	template: `
		<farmer-proofs limit="200"></farmer-proofs>
	`
}
const FarmerPlots = {
	template: `
		<div>
			<div class="my-2">
				<farmer-plots></farmer-plots>
			</div>
			<div class="my-2">
				<farmer-plot-dirs></farmer-plot-dirs>
			</div>
		</div>
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
			<v-tab to="/farmer">Farmer</v-tab>
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
	DisconnectedFromNode: "Disconnected from node",
	LoggedOff: "Logged off",
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

			if(this.status = AppStatus.LoggedOff || this.status == AppStatus.DisconnectedFromNode 
				|| this.status == AppStatus.Connecting) {
				await fetch('/server/session')
					.then( () => this.session_fails = 0 )
					.catch( () => this.session_fails++ );
			}

			if(this.status == AppStatus.Connecting || this.status == AppStatus.Syncing) {
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

			if(this.status == AppStatus.Connecting || this.status == AppStatus.Syncing 
				|| this.status == AppStatus.Synced) {
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
					.catch( () => {
						this.synced_fails++;
						this.$root.nodeInfo = null;
					} );				
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
		loggedIn() {
			return !this.$route.meta.is_login || this.$isWinGUI;
		},
		connectedToNetwork() {
			return this.peer_fails < 1;
		},
		synced() {
			return this.synced_fails == 0;
		},
		status() {
			let result = AppStatus.DisconnectedFromNode
			if(this.connectedToNode) {
				if(this.loggedIn) {					
					result = AppStatus.Connecting;
					if(this.connectedToNetwork) {
						result = AppStatus.Syncing;
					}

					if(this.synced) {
						result = AppStatus.Synced;
					}
				} else {
					result = AppStatus.LoggedOff;
				}
			}
			//console.log(result)
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

			<t-icon v-if="status == AppStatus.Connecting"
				color="red"
				:tooltip="AppStatus.Connecting">mdi-connection</t-icon>

			<t-icon v-else-if="status == AppStatus.Syncing"
				color="yellow darken-3" 
				:tooltip="AppStatus.Syncing">mdi-sync</t-icon>

			<t-icon v-else-if="status == AppStatus.Synced"
				color="green" 
				:tooltip="AppStatus.Synced">mdi-cloud-check</t-icon>

			<t-icon v-else-if="status == AppStatus.LoggedOff" 
				color="red" 
				:tooltip="AppStatus.LoggedOff">mdi-shield-key</t-icon>

			<t-icon v-else
				color="red" 
				:tooltip="AppStatus.DisconnectedFromNode">mdi-emoticon-dead</t-icon>

		</div>
		`
})

Vue.component('t-icon', {
	props: {
		color: String,
		tooltip: String
	},	
	template: `
		<v-tooltip bottom v-if="tooltip">
			<template v-slot:activator="{ on, attrs }">
				<v-icon :color="color" v-on="on"><slot /></v-icon>
			</template>
			<span>{{tooltip}}</span>
		</v-tooltip>	
		<v-icon :color="color" v-else><slot /></v-icon>		
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

