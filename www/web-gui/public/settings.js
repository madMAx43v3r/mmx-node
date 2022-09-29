
Vue.component('node-settings', {
	data() {
		return {
			error: null,
			result: null,
			loading: true,
			timelord: null,
			opencl_device: null,
			opencl_device_list: null,
			farmer_reward_addr: "null",
			timelord_reward_addr: "null",
			availableLanguages: availableLanguages,
			themes: [
				{ value: false, text: this.$t('node_settings.light') },
				{ value: true, text: this.$t('node_settings.dark') }
			]
		}
	},
	methods: {
		update() {
			fetch('/wapi/config/get')
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.timelord = data.timelord ? true : false;
					this.opencl_device = data.opencl_device ? data.opencl_device : 0;
					this.opencl_device_list = [{name: "None", value: -1}];
					let i = 0;
					for(const name of data.opencl_device_list) {
						this.opencl_device_list.push({name: name, value: i++});
					}
					this.farmer_reward_addr = data["Farmer.reward_addr"];
					this.timelord_reward_addr = data["TimeLord.reward_addr"];
				});
		},
		set_config(key, value, restart) {
			var req = {};
			req.key = key;
			req.value = value;
			fetch('/wapi/config/set', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						this.result = {key: req.key, value: req.value, restart};
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
	watch: {
		timelord(value, prev) {
			if(prev != null) {
				this.set_config("timelord", value, true);
			}
		},
		opencl_device(value, prev) {
			if(prev != null) {
				this.set_config("opencl_device", value, true);
			}
		},
		farmer_reward_addr(value, prev) {
			if(prev != "null") {
				this.set_config("Farmer.reward_addr", value.length ? value : null, true);
			}
		},
		timelord_reward_addr(value, prev) {
			if(prev != "null") {
				this.set_config("TimeLord.reward_addr", value.length ? value : null, true);
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
		},
		'$i18n.locale': async function (newVal, oldVal) {
			localStorage.setItem('language', newVal);
			await loadLanguageAsync(newVal);			
		},
		'$vuetify.theme.dark': async function (newVal, oldVal) {
			localStorage.setItem('theme_dark', newVal);
		}		
	},
	template: `
		<div>
			<v-card v-if="!$isWinGUI">
				<v-card-text>
					<v-select
						v-model="$i18n.locale"
						:label="$t('node_settings.language')"
						:items="availableLanguages" 
						item-text="language"
						item-value="code">
					</v-select>
					
					<v-select
						v-model="$vuetify.theme.dark"
						:label="$t('node_settings.theme')"
						:items="themes" 
						item-text="text"
						item-value="value">
					</v-select>

				</v-card-text>			
			</v-card>

			<v-card class="my-2">
				<v-card-text>

					<v-progress-linear :active="loading" indeterminate absolute top></v-progress-linear>

					<v-checkbox
						v-model="timelord"
						:label="$t('node_settings.enable_timelord')"
					></v-checkbox>

					<v-select
						v-model="opencl_device"
						label="OpenCL Device"
						:items="opencl_device_list"
						item-text="name"
						item-value="value"
					></v-select>

					<v-text-field
						:label="$t('node_settings.farmer_reward_address')"
						:placeholder="$t('common.reward_address_placeholder')"
						:value="farmer_reward_addr" @change="value => farmer_reward_addr = value"	
					></v-text-field>

					<v-text-field
						:label="$t('node_settings.timeLord_reward_address')"
						:placeholder="$t('common.reward_address_placeholder')"
						:value="timelord_reward_addr" @change="value => timelord_reward_addr = value"					
					></v-text-field>

				</v-card-text>
			</v-card>
							
			<v-alert
				border="left"
				colored-border
				type="success"
				v-if="result"
				elevation="2"
				class="my-2"
			>
				Set <b>{{result.key}}</b> to
				<b><template v-if="result.value != null"> '{{result.value}}' </template><template v-else> null </template></b>
				<template v-if="result.restart">{{ $t('node_settings.restart_needed') }}</template>
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

Vue.component('wallet-settings', {
	data() {
		return {
			error: null,
			result: null,
			loading: true,
			tokens: [],
			new_token_addr: null
		}
	},
	methods: {
		update() {
			fetch('/wapi/wallet/tokens')
				.then(response => response.json())
				.then(data => {this.tokens = data; this.loading = false;});
		},
		add_token(address) {
			fetch('/api/wallet/add_token?address=' + address)
				.then(response => {
					if(response.ok) {
						this.update();
						this.result = "Added token to whitelist: " + address;
					} else {
						response.text().then(data => {
							this.error = data;
						});
					}
				});
		},
		rem_token(address) {
			fetch('/api/wallet/rem_token?address=' + address)
				.then(response => {
					if(response.ok) {
						this.update();
						this.result = "Removed token from whitelist: " + address;
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
	watch: {
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
	// TODO: i18n
	template: `
		<div>
			<v-card class="my-2">
				<v-card-title>{{ $t('wallet_settings.token_whitelist') }}</v-card-title>
				<v-card-text>
					<v-progress-linear :active="loading" indeterminate absolute top></v-progress-linear>
					<v-simple-table>
						<thead>
						<tr>
							<th>{{ $t('wallet_settings.name') }}</th>
							<th>{{ $t('wallet_settings.symbol') }}</th>
							<th>{{ $t('wallet_settings.contract') }}</th>
							<th></th>
						</tr>
						</thead>
						<tbody>
						<template v-for="item in tokens" :key="item.currency">
							<template v-if="!item.is_native">
								<tr>
									<td>{{item.name}}</td>
									<td>{{item.symbol}}</td>
									<td><router-link :to="'/explore/address/' + item.currency">{{item.currency}}</router-link></td>
									<td><v-btn outlined text @click="rem_token(item.currency)">{{ $t('common.remove') }}</v-btn></td>
								</tr>
							</template>
						</template>
						</tbody>
					</v-simple-table>			
				</v-card-text>
			</v-card>

			<v-card>
				<v-card-text>
					<v-text-field
						:label="$t('wallet_settings.token_address')"
						v-model="new_token_addr" 
						placeholder="mmx1..."					
					></v-text-field>
					<v-btn @click="add_token(new_token_addr)" outlined color="primary">{{ $t('wallet_settings.add_token') }}</v-btn>
				</v-card-text>
			</v-card>
								
			<v-alert
				border="left"
				colored-border
				type="success"
				v-if="result"
				elevation="2"
				class="my-2"
			>
				<b>{{result}}</b>
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
