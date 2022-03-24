
app.component('node-menu', {
	template: `
		<div class="ui large pointing menu">
			<router-link class="item" :class="{active: $route.meta.page == 'peers'}" to="/node/peers">Peers</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'blocks'}" to="/node/blocks">Blocks</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'netspace'}" to="/node/netspace">Netspace</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'vdf_speed'}" to="/node/vdf_speed">VDF Speed</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'reward'}" to="/node/reward">Block Reward</router-link>
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
					<div class="value">{{data.is_synced ? "Yes": "No"}}</div>
					<div class="label">Synced</div>
				</div>
				<div class="statistic">
					<div class="value">{{data.height}}</div>
					<div class="label">Height</div>
				</div>
				<div class="statistic">
					<div class="value">{{(data.total_space / Math.pow(1000, 5)).toFixed(3)}} PB</div>
					<div class="label">Netspace</div>
				</div>
				<div class="statistic">
					<div class="value">{{(data.total_supply / Math.pow(10, 6)).toFixed(0)}} MMX</div>
					<div class="label">Supply</div>
				</div>
			</div>
		</div>
		<div class="ui segment" v-if="data">
			<div class="ui four tiny statistics">
				<div class="statistic">
					<div class="value">{{data.address_count}}</div>
					<div class="label">No. Address</div>
				</div>
				<div class="statistic">
					<div class="value">{{data.utxo_count}}</div>
					<div class="label">No. UTXO</div>
				</div>
				<div class="statistic">
					<div class="value">{{(data.time_diff / Math.pow(10, 4)).toFixed(3)}} M/s</div>
					<div class="label">VDF Speed</div>
				</div>
				<div class="statistic">
					<div class="value">{{(data.block_reward.value).toFixed(3)}} MMX</div>
					<div class="label">Block Reward</div>
				</div>
			</div>
		</div>
		`
})

app.component('node-peers', {
	data() {
		return {
			data: null,
			timer: null,
			gdata:[{
		        x: [1,2,3,4],
		        y: [10,15,13,17],
		        type:"scatter"
		      }],
		      layout:{
		        title: "My graph"
		      }
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
		<table class="ui definition table striped" v-if="data">
			<thead>
			<tr>
				<th>IP</th>
				<th>Height</th>
				<th>Type</th>
				<th>Version</th>
				<th>Download</th>
				<th>Upload</th>
				<th>Ping</th>
				<th>Duration</th>
				<th>Credits</th>
				<th>TX Gas</th>
				<th>Connection</th>
			</tr>
			</thead>
			<tbody>
			<tr v-for="item in data.peers" :key="item.address">
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
				<td>{{item.is_outbound ? "outbound" : "inbound"}}</td>
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
				title: "Netspace (PB)",
				xaxis: {
					title: "Height"
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
				title: "VDF Speed (M/s)",
				xaxis: {
					title: "Height"
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
				title: "Block Reward (MMX)",
				xaxis: {
					title: "Height"
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



