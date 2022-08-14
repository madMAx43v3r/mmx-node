
Vue.component('node-menu', {
	template: `
		<v-tabs>
			<v-tab to="/node/log">{{ $t('node_menu.log') }}</v-tab>
			<v-tab to="/node/peers">{{ $t('node_menu.peers') }}</v-tab>
			<v-tab to="/node/blocks">{{ $t('node_menu.blocks') }}</v-tab>
			<v-tab to="/node/netspace">{{ $t('node_menu.netspace') }}</v-tab>
			<v-tab to="/node/vdf_speed">{{ $t('node_menu.vdf_speed') }}</v-tab>
			<v-tab to="/node/reward">{{ $t('node_menu.block_reward') }}</v-tab>
		</v-tabs>
		`
})

Vue.component('node-info', {
	data() {
		return {
			data: null
		}
	},
	methods: {
		update() {
			fetch('/wapi/node/info')
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 5000);
	},
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<div v-if="data">
		<v-container fluid id="features" class="pb-0">

			<v-col cols="12">
				<v-row align="center" justify="space-around">
					<v-col cols="12" xl="3" md="3" sm="6" class="text-center">
						<v-card>
							<v-card-title>{{data.is_synced ? $t('common.yes') : $t('common.no') }}</v-card-title>
							<v-card-text>{{ $t('node_info.synced') }}</v-card-text>
						</v-card>
					</v-col>
					<v-col cols="12" xl="3" md="3" sm="6" class="text-center">					
						<v-card>
							<v-card-title>{{data.height}}</v-card-title>
							<v-card-text>{{ $t('node_info.height') }}</v-card-text>
						</v-card>
					</v-col>
					<v-col cols="12" xl="3" md="3" sm="6" class="text-center">	
						<v-card>
							<v-card-title>{{(data.total_space / Math.pow(1000, 5)).toFixed(3)}} PB</v-card-title>
							<v-card-text>{{ $t('node_info.netspace') }}</v-card-text>
						</v-card>
					</v-col>
					<v-col cols="12" xl="3" md="3" sm="6" class="text-center">
						<v-card>
							<v-card-title>{{(data.total_supply / Math.pow(10, 6)).toFixed(0)}} MMX</v-card-title>
							<v-card-text>{{ $t('node_info.supply') }}</v-card-text>
						</v-card>
					</v-col>
				</v-row>
				<v-row align="center" justify="space-around">
					<v-col cols="12" xl="3" md="3" sm="6" class="text-center">
						<v-card>
							<v-card-title>{{data.address_count}}</v-card-title>
							<v-card-text>{{ $t('node_info.no_addresss') }}</v-card-text>
						</v-card>
					</v-col>
					<v-col cols="12" xl="3" md="3" sm="6" class="text-center">
						<v-card>
							<v-card-title>{{(data.block_size * 100).toFixed(2)}} %</v-card-title>
							<v-card-text>{{ $t('node_info.block_size') }}</v-card-text>
						</v-card>
					</v-col>
					<v-col cols="12" xl="3" md="3" sm="6" class="text-center">
						<v-card>
							<v-card-title>{{(data.time_diff / 8 / Math.pow(10, 3)).toFixed(3)}} M/s</v-card-title>
							<v-card-text>{{ $t('node_info.vdf_speed') }}</v-card-text>
						</v-card>
					</v-col>
					<v-col cols="12" xl="3" md="3" sm="6" class="text-center">
						<v-card>
							<v-card-title>{{(data.block_reward.value).toFixed(3)}} MMX</v-card-title>
							<v-card-text>{{ $t('node_info.block_reward') }}</v-card-text>
						</v-card>
					</v-col>													
				</v-row>
			</v-col>

		</v-container>
		</div>
		`
})

Vue.component('node-peers', {
	data() {
		return {
			data: null,
			timer: null
		}
	},
	methods: {
		update() {
			fetch('/api/router/get_peer_info')
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 5000);
	},
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<table class="ui compact definition table striped" v-if="data">
			<thead>
			<tr>
				<th>{{ $t('node_peers.ip') }}</th>
				<th>{{ $t('node_peers.height') }}</th>
				<th>{{ $t('node_peers.type') }}</th>
				<th>{{ $t('node_peers.version') }}</th>
				<th>{{ $t('node_peers.received') }}</th>
				<th>{{ $t('node_peers.send') }}</th>
				<th>{{ $t('node_peers.ping') }}</th>
				<th>{{ $t('node_peers.duration') }}</th>
				<th>{{ $t('node_peers.credits') }}</th>
				<th>{{ $t('node_peers.connection') }}</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data.peers">
				<td>{{item.address}}</td>
				<td>{{item.is_synced ? "" : "!"}}{{item.height}}</td>
				<td>{{item.type}}</td>
				<td>{{(item.version / 100).toFixed()}}.{{item.version % 100}}</td>
				<td><b>{{(item.bytes_recv / item.connect_time_ms / 1.024).toFixed(1)}}</b> KB/s</td>
				<td><b>{{(item.bytes_send / item.connect_time_ms / 1.024).toFixed(1)}}</b> KB/s</td>
				<td><b>{{item.ping_ms}}</b> ms</td>
				<td><b>{{(item.connect_time_ms / 1000 / 60).toFixed()}}</b> min</td>
				<td>{{item.credits}}</td>
				<td>{{item.is_outbound ? $t('node_peers.outbound') : $t('node_peers.inbound') }}</td>
			</tr>
			</tbody>
		</table>
		`
})

Vue.component('netspace-graph', {
	props: {
		limit: Number,
		step: Number
	},
	data() {
		return {
			data: null,
			layout: {
				title: this.$t('netspace_graph.title', ['PB']),
				xaxis: {
					title: this.$t('common.height')
				},
				yaxis: {
					title: "PB"
				}
			},
			timer: null,
			loading: false
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/node/graph/blocks?limit=' + this.limit + '&step=' + (this.step ? this.step : 90))
				.then(response => response.json())
				.then(data => {
					const tmp = {x: [], y: [], type: "scatter"};
					for(const item of data) {
						tmp.x.push(item.height);
						tmp.y.push(item.netspace);
					}
					this.loading = false;
					this.data = [tmp];
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 60 * 1000);
	},
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<template v-if="!data && loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<template v-if="data">
			<div class="ui segment">
				<vue-plotly :data="data" :layout="layout" :display-mode-bar="false"></vue-plotly>
			</div>
		</template>
		`
})

Vue.component('vdf-speed-graph', {
	props: {
		limit: Number,
		step: Number
	},
	data() {
		return {
			data: null,
			layout: {
				title: this.$t('vdf_speed_graph.title', ['M/s']),
				xaxis: {
					title: this.$t('common.height')
				},
				yaxis: {
					title: "M/s"
				}
			},
			timer: null,
			loading: false
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/node/graph/blocks?limit=' + this.limit + '&step=' + (this.step ? this.step : 90))
				.then(response => response.json())
				.then(data => {
					const tmp = {x: [], y: [], type: "scatter"};
					for(const item of data) {
						tmp.x.push(item.height);
						tmp.y.push(item.vdf_speed);
					}
					this.loading = false;
					this.data = [tmp];
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 60 * 1000);
	},
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<template v-if="!data && loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<template v-if="data">
			<div class="ui segment">
				<vue-plotly :data="data" :layout="layout" :display-mode-bar="false"></vue-plotly>
			</div>
		</template>
		`
})

Vue.component('block-reward-graph', {
	props: {
		limit: Number,
		step: Number
	},
	data() {
		return {
			base_data: null,
			fee_data: null,
			base_layout: {
				title: this.$t('block_reward_graph.title', ['MMX']),
				xaxis: {
					title: this.$t('common.height')
				},
				yaxis: {
					title: "MMX"
				}
			},
			fee_layout: {
				title: this.$t('tx_fees_graph.title', ['MMX']),
				xaxis: {
					title: this.$t('common.height')
				},
				yaxis: {
					title: "MMX"
				}
			},
			timer: null,
			loading: false
		}
	},
	methods: {
		update() {
			this.loading = true;
			fetch('/wapi/node/graph/blocks?limit=' + this.limit + '&step=' + (this.step ? this.step : 90))
				.then(response => response.json())
				.then(data => {
					{
						const tmp = {x: [], y: [], type: "scatter"};
						for(const item of data) {
							tmp.x.push(item.height);
							tmp.y.push(item.base_reward);
						}
						this.base_data = [tmp];
					}
					{
						const tmp = {x: [], y: [], type: "scatter"};
						for(const item of data) {
							if(item.tx_fees > 0) {
								tmp.x.push(item.height);
								tmp.y.push(item.tx_fees);
							}
						}
						this.fee_data = [tmp];
					}
					this.loading = false;
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 60 * 1000);
	},
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<template v-if="!base_data && loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<template v-if="base_data">
			<div class="ui segment">
				<vue-plotly :data="base_data" :layout="base_layout" :display-mode-bar="false"></vue-plotly>
			</div>
		</template>
		<template v-if="!fee_data && loading">
			<div class="ui basic loading placeholder segment"></div>
		</template>
		<template v-if="fee_data">
			<div class="ui segment">
				<vue-plotly :data="fee_data" :layout="fee_layout" :display-mode-bar="false"></vue-plotly>
			</div>
		</template>
		`
})

Vue.component('node-log', {
	data() {
		return {
			limit: 100,
			level: 3,
			module: null,
			currentItem: null
		}
	},
	template: `
		<div>
			<v-tabs v-model="currentItem">
				<v-tab @click="module = null">{{ $t('node_log.terminal') }}</v-tab>
				<v-tab @click="module = 'Node'">{{ $t('node_log.node') }}</v-tab>
				<v-tab @click="module = 'Router'">{{ $t('node_log.router') }}</v-tab>
				<v-tab @click="module = 'Wallet'">{{ $t('node_log.wallet') }}</v-tab>
				<v-tab @click="module = 'Farmer'">{{ $t('node_log.farmer') }}</v-tab>
				<v-tab @click="module = 'Harvester'">{{ $t('node_log.harvester') }}</v-tab>
				<v-tab @click="module = 'TimeLord'">{{ $t('node_log.timelord') }}</v-tab>
			</v-tabs>

	  		<node-log-table :limit="limit" :level="level" :module="module"></node-log-table>

		</div>
		
		`
})

Vue.component('node-log-table', {
	props: {
		limit: Number,
		level: Number,
		module: String
	},
	data() {
		return {
			data: null,
			timer: null,
		}
	},
	methods: {
		update() {
			fetch('/wapi/node/log?limit=' + this.limit + "&level=" + this.level + (this.module ? "&module=" + this.module : ""))
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	watch: {
		limit() {
			this.update();
		},
		level() {
			this.update();
		},
		module() {
			this.update();
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 2000);
	},
	unmounted() {
		clearInterval(this.timer);
	},
	template: `
		<v-table>
			<tbody>
				<tr v-for="item in data" :key="item.time">
					<td><code>{{new Date(item.time / 1000).toLocaleTimeString()}}</code></td>
					<td><code>{{ item.module }}</code></td>
					<td><code>{{ item.message }}</code></td>
				</tr>
			</tbody>
		</v-table>
		`
})

