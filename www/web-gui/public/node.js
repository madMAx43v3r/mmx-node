
app.component('node-menu', {
	template: `
		<div class="ui large pointing menu">
			<router-link class="item" :class="{active: $route.meta.page == 'peers'}" to="/node/peers">Peers</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'blocks'}" to="/node/blocks">Blocks</router-link>
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





