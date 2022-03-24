
app.component('node-menu', {
	template: `
		<div class="ui large pointing menu">
			<router-link class="item" :class="{active: $route.meta.page == 'info'}" to="/node/info/">Info</router-link>
			<router-link class="item" :class="{active: $route.meta.page == 'peers'}" to="/node/peers/">Peers</router-link>
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
		<explore-blocks :limit="10"></explore-blocks>
		`
})







