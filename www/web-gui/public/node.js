
app.component('node-menu', {
	template: `
		<div class="ui large pointing menu">
			<router-link class="item" :class="{active: $route.meta.page == 'log'}" to="/node/log">{{ $t('node_menu.log') }}</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'peers'}" to="/node/peers">{{ $t('node_menu.peers') }}</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'blocks'}" to="/node/blocks">{{ $t('node_menu.blocks') }}</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'netspace'}" to="/node/netspace">{{ $t('node_menu.netspace') }}</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'vdf_speed'}" to="/node/vdf_speed">{{ $t('node_menu.vdf_speed') }}</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'reward'}" to="/node/reward">{{ $t('node_menu.block_reward') }}</router-link>
		</div>
		`
})

app.component('node-info', {
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
		<div class="ui segment" v-if="data">
			<div class="ui four tiny statistics">
				<div class="statistic">
					<div class="value">{{data.is_synced ? $t('common.yes') : $t('common.no') }}</div>
					<div class="label">{{ $t('node_info.synced') }}</div>
				</div>
				<div class="statistic">
					<div class="value">{{data.height}}</div>
					<div class="label">{{ $t('node_info.height') }}</div>
				</div>
				<div class="statistic">
					<div class="value">{{(data.total_space / Math.pow(1000, 5)).toFixed(3)}} PB</div>
					<div class="label">{{ $t('node_info.netspace') }}</div>
				</div>
				<div class="statistic">
					<div class="value">{{(data.total_supply / Math.pow(10, 6)).toFixed(0)}} MMX</div>
					<div class="label">{{ $t('node_info.supply') }}</div>
				</div>
			</div>
		</div>
		<div class="ui segment" v-if="data">
			<div class="ui four tiny statistics">
				<div class="statistic">
					<div class="value">{{data.address_count}}</div>
					<div class="label">{{ $t('node_info.no_addresss') }}</div>
				</div>
				<div class="statistic">
				</div>
				<div class="statistic">
					<div class="value">{{(data.time_diff / 8 / Math.pow(10, 3)).toFixed(3)}} M/s</div>
					<div class="label">{{ $t('node_info.vdf_speed') }}</div>
				</div>
				<div class="statistic">
					<div class="value">{{(data.block_reward.value).toFixed(3)}} MMX</div>
					<div class="label">{{ $t('node_info.block_reward') }}</div>
				</div>
			</div>
		</div>
		`
})

app.component('node-peers', {
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
				<th>{{ $t('node_peers.tx_gas') }}</th>
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
				<td><b>{{(item.tx_credits / Math.pow(10, 6)).toFixed(3)}}</b> MMX</td>
				<td>{{item.is_outbound ? $t('node_peers.outbound') : $t('node_peers.inbound') }}</td>
			</tr>
			</tbody>
		</table>
		`
})

app.component('netspace-graph', {
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

app.component('vdf-speed-graph', {
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

app.component('block-reward-graph', {
	props: {
		limit: Number,
		step: Number
	},
	data() {
		return {
			data: null,
			layout: {
				title: this.$t('block_reward_graph.title', ['MMX']),
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
					const tmp = {x: [], y: [], type: "scatter"};
					for(const item of data) {
						if(item.reward > 0) {
							tmp.x.push(item.height);
							tmp.y.push(item.reward);
						}
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

app.component('node-log', {
	data() {
		return {
			limit: 100,
			level: 3,
			module: null
		}
	},
	template: `
		<div class="ui menu">
			<div class="link item" :class="{active: !module}" @click="module = null">{{ $t('node_log.terminal') }}</div>
			<div class="link item" :class="{active: module == 'Node'}" @click="module = 'Node'">{{ $t('node_log.node') }}</div>
			<div class="link item" :class="{active: module == 'Router'}" @click="module = 'Router'">{{ $t('node_log.router') }}</div>
			<div class="link item" :class="{active: module == 'Wallet'}" @click="module = 'Wallet'">{{ $t('node_log.wallet') }}</div>
			<div class="link item" :class="{active: module == 'Farmer'}" @click="module = 'Farmer'">{{ $t('node_log.farmer') }}</div>
			<div class="link item" :class="{active: module == 'Harvester'}" @click="module = 'Harvester'">{{ $t('node_log.harvester') }}</div>
			<div class="link item" :class="{active: module == 'TimeLord'}" @click="module = 'TimeLord'">{{ $t('node_log.timelord') }}</div>
		</div>
		<node-log-table :limit="limit" :level="level" :module="module"></node-log-table>
		`
})

app.component('node-log-table', {
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
		<table class="ui small very compact table striped" v-if="data">
			<tbody>
			<tr v-for="item in data" :key="item.time" :class="{error: item.level == 1, warning: item.level == 2}">
				<td class="collapsing"><code>{{new Date(item.time / 1000).toLocaleTimeString()}}</code></td>
				<td><code><b>{{item.module}}</b></code></td>
				<td><code>{{item.message}}</code></td>
			</tr>
			</tbody>
		</table>
		`
})

