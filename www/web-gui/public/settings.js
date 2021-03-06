
app.component('node-settings', {
	data() {
		return {
			error: null,
			result: null,
			loading: true,
			timelord: null,
			farmer_reward_addr: "null",
			timelord_reward_addr: "null",
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
		}
	},
	template: `
		<template v-if="loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<template v-else>
			<template v-if="!isWinGUI">
				<div class="ui segment">
					<form class="ui form">
						<div class="field">
							<label>{{ $t('node_settings.language') }}</label>
							<select v-model="$i18n.locale">
								<option v-for="(language, code) in availableLanguages" :value="code">{{language}}</option>
							</select>
						</div>
					</form>
				</div>
			</template>
			<div class="ui segment">
				<form class="ui form">
					<div class="field">
						<div class="ui checkbox">
							<input type="checkbox" v-model="timelord">
							<label>{{ $t('node_settings.enable_timelord') }}</label>
						</div>
					</div>
				</form>
			</div>
			<div class="ui segment">
				<form class="ui form">
					<div class="field">
						<label>{{ $t('node_settings.farmer_reward_address') }}</label>
						<input type="text" v-model.lazy="farmer_reward_addr" :placeholder="$t('common.reward_address_placeholder')"/>
					</div>
					<div class="field">
						<label>{{ $t('node_settings.timeLord_reward_address') }}</label>
						<input type="text" v-model.lazy="timelord_reward_addr" :placeholder="$t('common.reward_address_placeholder')"/>
					</div>
				</form>
			</div>
		</template>
		<div class="ui message" v-if="result">
			Set <b>{{result.key}}</b> to
			<b><template v-if="result.value != null"> '{{result.value}}' </template><template v-else> null </template></b>
			<template v-if="result.restart">{{ $t('node_settings.restart_needed') }}</template>
		</div>
		<div class="ui negative message" v-if="error">
			{{ $t('common.failed_with') }}: <b>{{error}}</b>
		</div>
		`
})

app.component('wallet-settings', {
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
	template: `
		<template v-if="loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<template v-else>
			<div class="ui segment">
				<form class="ui form">
					<div class="field">
						<label>Token Whitelist</label>
						<table class="ui table striped">
							<thead>
							<tr>
								<th>Name</th>
								<th>Symbol</th>
								<th>Contract</th>
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
										<td><div class="ui tiny button" @click="rem_token(item.currency)">Remove</div></td>
									</tr>
								</template>
							</template>
							</tbody>
						</table>
					</div>
					<div class="ui segment">
						<div class="two field">
							<div class="field">
								<label>Token Address</label>
								<input type="text" v-model="new_token_addr" placeholder="mmx1..."/>
							</div>
							<div class="field">
								<div class="ui primary button" @click="add_token(new_token_addr)">Add Token</div>
							</div>
						</div>
					</div>
				</form>
			</div>
		</template>
		<div class="ui message" v-if="result">
			<b>{{result}}</b>
		</div>
		<div class="ui negative message" v-if="error">
			{{ $t('common.failed_with') }}: <b>{{error}}</b>
		</div>
		`
})
