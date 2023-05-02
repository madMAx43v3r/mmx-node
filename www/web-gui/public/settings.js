
Vue.component('node-settings', {
	data() {
		return {
			error: null,
			result: null,
			loading: true,
			timelord: null,
			open_port: null,
			opencl_device: null,
			opencl_platform: null,
			opencl_device_list: null,
			opencl_platform_list: null,
			farmer_reward_addr: "null",
			timelord_reward_addr: "null",
			enable_timelord_reward: null,
			verify_timelord_reward: null,
			reload_interval: null,
			plot_dirs: [],
			new_plot_dir: null,
			revert_height: null,
			availableLanguages: availableLanguages
		}
	},
	methods: {
		update() {
			fetch('/wapi/config/get')
				.then(response => response.json())
				.then(data => {
					this.loading = false;
					this.timelord = data.timelord ? true : false;
					this.open_port = data["Router.open_port"] ? true : false;
					this.opencl_platform = data["opencl.platform"] != null ? data["opencl.platform"] : "";
					this.opencl_device = data["Node.opencl_device"] != null ? data["Node.opencl_device"] : 0;
					this.opencl_device_list = [{name: "None", value: -1}];
					{
						let list = data["opencl.device_list"];
						if(list) {
							let i = 0;
							for(const name of list) {
								this.opencl_device_list.push({name: name, value: i++});
							}
						} else {
							this.opencl_device_list.push({name: "First device found", value: 0});
						}
					}
					this.opencl_platform_list = [];
					{
						let list = data["opencl.platform_list"];
						if(list) {
							for(const name of list) {
								this.opencl_platform_list.push({name: name, value: name});
							}
						} else {
							this.opencl_platform_list.push({name: "First platform found", value: ""});
						}
					}
					this.farmer_reward_addr = data["Farmer.reward_addr"];
					this.timelord_reward_addr = data["TimeLord.reward_addr"];
					this.enable_timelord_reward = data["TimeLord.enable_reward"];
					this.verify_timelord_reward = data["Node.verify_vdf_rewards"];
					this.reload_interval = data["Harvester.reload_interval"];
					this.plot_dirs = data["Harvester.plot_dirs"];
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
		},
		add_plot_dir(path) {
			if(path == null || path == "") {
				return;
			}
			var req = {};
			req.path = path;
			fetch('/api/harvester/add_plot_dir', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						this.update();
						this.result = {key: "Harvester.plot_dirs", value: "+ " + req.path, restart: false};
					} else {
						response.text().then(data => {
							this.error = data;
						});
					}
				});
		},
		rem_plot_dir(path) {
			var req = {};
			req.path = path;
			fetch('/api/harvester/rem_plot_dir', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						this.update();
						this.result = {key: "Harvester.plot_dirs", value: "- " + req.path, restart: false};
					} else {
						response.text().then(data => {
							this.error = data;
						});
					}
				});
		},
		revert_sync(height) {
			if(height == null) {
				return;
			}
			height = parseInt(height)
			if(!(height >= 0)) {
				this.error = "invalid height";
				return;
			}
			var req = {};
			req.height = parseInt(height);
			fetch('/api/node/revert_sync', {body: JSON.stringify(req), method: "post"})
				.then(response => {
					if(response.ok) {
						this.result = {key: "Node.revert_sync", value: height, restart: false};
					} else {
						response.text().then(data => {
							this.error = data;
						});
					}
					this.revert_height = null;
				});
		}
	},
	created() {
		this.update();
	},
	computed: {
		themes() {
			return [ 
					{ value: false, text: this.$t('node_settings.light') },
					{ value: true, text: this.$t('node_settings.dark') }
				];
		}						
	},
	watch: {
		timelord(value, prev) {
			if(prev != null) {
				this.set_config("timelord", value, true);
			}
		},
		open_port(value, prev) {
			if(prev != null) {
				this.set_config("Router.open_port", value, true);
			}
		},
		opencl_platform(value, prev) {
			if(prev != null) {
				this.set_config("opencl.platform", value, true);
			}
		},
		opencl_device(value, prev) {
			if(prev != null) {
				this.set_config("Node.opencl_device", value, true);
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
		enable_timelord_reward(value, prev) {
			if(prev != null) {
				this.set_config("TimeLord.enable_reward", value, true);
			}
		},
		verify_timelord_reward(value, prev) {
			if(prev != null) {
				this.set_config("Node.verify_vdf_rewards", value, true);
			}
		},
		reload_interval(value, prev) {
			if(prev != null) {
				this.set_config("Harvester.reload_interval", value, true);
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
				<v-card-title>{{ $t('node_settings.gui') }}</v-card-title>
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
				<v-card-title>{{ $t('node_settings.general') }}</v-card-title>
				<v-card-text>
					<v-progress-linear :active="loading" indeterminate absolute top></v-progress-linear>
					
					<v-checkbox
						v-model="timelord"
						:label="$t('node_settings.enable_timelord')"
						class="my-0"
					></v-checkbox>
					<v-checkbox
						:disabled="!timelord"
						v-model="enable_timelord_reward"
						:label="$t('node_settings.enable_timelord_reward')"
						class="my-0"
					></v-checkbox>
					<v-checkbox
						v-model="verify_timelord_reward"
						:label="$t('node_settings.verify_timelord_reward')"
						class="my-0"
					></v-checkbox>
					<v-checkbox
						v-model="open_port"
						:label="$t('node_settings.open_port')"
						class="my-0"
					></v-checkbox>
					
					<v-select
						v-model="opencl_platform"
						label="OpenCL Platform"
						:items="opencl_platform_list"
						item-text="name"
						item-value="value"
					></v-select>

					<v-select
						v-model="opencl_device"
						:label="$t('node_settings.opencl_device')"
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
			
			<v-card class="my-2">
				<v-card-title>{{ $t('node_settings.blockchain') }}</v-card-title>
				<v-card-text>
					<v-text-field
						:label="$t('node_settings.revert_db_to_height')"
						v-model="revert_height"
					></v-text-field>
					<v-btn @click="revert_sync(revert_height)" outlined color="error">{{ $t('node_settings.revert') }}</v-btn>
				</v-card-text>
			</v-card>
			
			<v-card class="my-2">
				<v-card-title>{{ $t('harvester_settings.harvester') }}</v-card-title>
				<v-card-text>
					<v-progress-linear :active="loading" indeterminate absolute top></v-progress-linear>
					
					<v-text-field
						:label="$t('harvester_settings.harvester_reload_interval')"
						:value="reload_interval" @change="value => reload_interval = parseInt(value)"
					></v-text-field>
					
					<v-card class="my-2">
						<v-simple-table>
							<thead>
								<tr>
									<th>{{ $t('harvester_settings.plot_directory') }}</th>
									<th></th>
								</tr>
							</thead>
							<tbody>
								<tr v-for="item in plot_dirs" :key="item">
									<td>{{item}}</td>
									<td><v-btn outlined text @click="rem_plot_dir(item)">Remove</v-btn></td>
								</tr>
							</tbody>
						</v-simple-table>
					</v-card>
					
					<v-text-field
						:label="$t('harvester_settings.plot_directory')"
						v-model="new_plot_dir"
					></v-text-field>
					<v-btn @click="add_plot_dir(new_plot_dir)" outlined color="primary">{{ $t('harvester_settings.add_directory') }}</v-btn>
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
					
					<v-card class="my-2">
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
					</v-card>
					
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
