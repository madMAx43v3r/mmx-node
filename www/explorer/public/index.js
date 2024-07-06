
const WAPI_URL = "https://rpc.mmx.network";

function validate_address(address) {
	return address && address.length == 62 && address.startsWith("mmx1");
}

function get_short_addr(address, length) {
	if(!length) {
		length = 10;
	}
	return address.substring(0, length) + '...' + address.substring(62 - length);
}

function get_short_hash(hash, length) {
	if(!length) {
		length = 10;
	}
	return hash.substring(0, length) + '...' + hash.substring(64 - length);
}

function get_tx_type_color(type, dark = false) {
	if(type == "REWARD") return dark ? "lime--text" : "lime--text text--darken-2";
	if(type == "RECEIVE" || type == "REWARD" || type == "VDF_REWARD") return "green--text";
	if(type == "SPEND") return "red--text";
	if(type == "TXFEE") return "grey--text";
	return "";
}

const MMX_ADDR = "mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev";

const Explore = {
	template: `
		<div>
			<explore-menu></explore-menu>
			<router-view></router-view>
		<div>
	`
}
const ExploreBlocks = {
	template: `
		<explore-blocks :limit="20"></explore-blocks>
	`
}
const ExploreTransactions = {
	template: `
		<explore-transactions :limit="100"></explore-transactions>
	`
}
const ExploreFarmers = {
	template: `
		<explore-farmers :limit="200"></explore-farmers>
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

Vue.component('main-menu', {
	methods: {
		toggle_dark_mode() {
			const value = !this.$vuetify.theme.dark;
			this.$vuetify.theme.dark = value;
			localStorage.setItem('theme_dark', value);
		}
	},
	template: `
		<v-tabs>
			<v-tab to="/explore">{{ $t('main_menu.explore') }}</v-tab>
			<v-spacer></v-spacer>
			<v-btn icon color="info" @click="toggle_dark_mode">
				<v-icon class="m-1">{{$vuetify.theme.dark ? 'mdi-moon-waxing-crescent' : 'mdi-white-balance-sunny'}}</v-icon>
			</v-btn>
		</v-tabs>
		`
})

Vue.component('app', {
	computed: {
		colClass(){
			return 'cols-12 col-xl-8 offset-xl-2';
		},
		fluid() {
			return !(this.$vuetify.breakpoint.xl || this.$vuetify.breakpoint.lg);
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

