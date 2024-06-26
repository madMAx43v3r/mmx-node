
Vue.component('farmer-menu', {
	template: `
		<v-tabs>
			<v-tab to="/farmer/plots">{{ $t('farmer_menu.plots') }}</v-tab>
			<v-tab to="/farmer/blocks">{{ $t('farmer_menu.blocks') }}</v-tab>
			<v-tab to="/farmer/proofs">{{ $t('farmer_menu.proofs') }}</v-tab>
		</v-tabs>
		`
})

Vue.component('farmer-info', {
	data() {
		return {
			data: null
		}
	},
	methods: {
		update() {
			fetch('/wapi/farmer/info')
				.then(response => response.json())
				.then(data => this.data = data);
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 10000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<div>
			<v-row class="my-2 py-0">
				<v-col cols="12" class="my-0 py-0">
					<v-card min-height="90">
						<v-card-title>
							<v-row align="center" justify="space-around">

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">
									<v-row align="center" justify="space-around">
										<div v-if="data">{{ (data.total_bytes / Math.pow(1000, 4)).toFixed(3) }} TB</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"></v-skeleton-loader>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										 {{ $t('farmer_info.physical_size') }}
									</v-row>
								</v-col>
								
								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">
									<v-row align="center" justify="space-around">
										<div v-if="data">{{ (data.total_bytes_effective / Math.pow(1000, 4)).toFixed(3) }} TBe</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"></v-skeleton-loader>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										 Effective Size
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										<div v-if="data">{{ (data.total_virtual_bytes / Math.pow(1000, 4)).toFixed(3) }} TBe</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"></v-skeleton-loader>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										{{ $t('farmer_info.virtual_size') }}
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										<div v-if="data">{{ ((data.total_bytes_effective + data.total_virtual_bytes) / Math.pow(1000, 4)).toFixed(3) }} TBe</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"></v-skeleton-loader>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										{{ $t('farmer_info.total_farm_size') }}
									</v-row>
								</v-col>

							</v-row>
						</v-card-title>
					</v-card>
				</v-col>
			</v-row>
		</div>
		`
})

Vue.component('farmer-rewards', {
	data() {
		return {
			data: null,
			netspace: null,
			farm_size: null
		}
	},
	methods: {
		update() {
			fetch('/wapi/farmer/blocks/summary')
				.then(response => response.json())
				.then(data => this.data = data);
			fetch('/wapi/node/info')
				.then(response => response.json())
				.then(data => this.netspace = data.total_space);
			fetch('/wapi/farmer/info')
				.then(response => response.json())
				.then(data => this.farm_size = data.total_bytes_effective + data.total_virtual_bytes);
		}
	},
	computed: {
		est_time_to_win() {
			if(this.netspace && this.farm_size) {
				const est_min = (this.netspace / this.farm_size) / 6;
				if(est_min < 120) {
					return Math.floor(est_min) + " minutes";
				}
				const est_hours = est_min / 60;
				if(est_hours < 48) {
					return Math.floor(est_hours) + " hours";
				}
				const est_days = est_hours / 24;
				return Math.floor(est_days) + " days";
			}
			return "N/A";
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 30000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<v-card class="my-2">
		<v-card-text>
			<template v-if="data">
				<v-card class="my-2">
					<v-simple-table>
						<tbody>
						<tr>
							<td class="key-cell">No. Blocks</td>
							<td>{{data.num_blocks}}</td>
						</tr>
						<tr>
							<td class="key-cell">Last Height</td>
							<td>{{data.last_height > 0 ? data.last_height : "N/A"}}</td>
						</tr>
						<tr>
							<td class="key-cell">Total Rewards</td>
							<td><b>{{data.total_rewards_value}}</b>&nbsp; MMX</td>
						</tr>
						<tr>
							<td class="key-cell">Time to win</td>
							<td>{{est_time_to_win}} (on average)</td>
						</tr>
						</tbody>
					</v-simple-table>
				</v-card>
				<v-card class="my-2">
					<v-simple-table>
						<thead>
						<tr>
							<th colspan="2">Reward</th>
							<th>{{ $t('transaction_view.address') }}</th>
						</tr>
						</thead>
						<tbody>
						<tr v-for="item in data.rewards" :key="item.address">
							<td class="collapsing"><b>{{item.value}}</b></td>
							<td>MMX</td>
							<td><router-link :to="'/explore/address/' + item.address">{{item.address}}</router-link></td>
						</tr>
						</tbody>
					</v-simple-table>
				</v-card>
			</template>
		</v-card-text>
		</v-card>
		`
})

Vue.component('farmer-plots', {
	data() {
		return {
			loaded: false,
			plot_count: [],
			harvester_bytes: []
		}
	},
	computed: {
		headers() {
			return [
				{ text: this.$t('farmer_plots.type'), value: 'ksize', width: "10%" },
				{ text: this.$t('farmer_plots.count'), value: 'count' },
			]
		},
		headers2() {
			return [
				{ text: this.$t('common.harvester'), value: 'name' },
				{ text: this.$t('farmer_info.physical_size'), value: 'bytes' },
				{ text: "Effective Size", value: 'effective' },
			]
		}
	},
	methods: {
		update() {
			fetch('/wapi/farmer/info')
				.then(response => response.json())
				.then(data => {
					this.plot_count = [];
					for(const entry of data.plot_count) {
						this.plot_count.push({ksize: entry[0], count: entry[1]});
					}
					this.harvester_bytes = [];
					for(const entry of data.harvester_bytes) {
						this.harvester_bytes.push({name: entry[0], bytes: entry[1][0], effective: entry[1][1]});
					}
					this.loaded = true;
				});
		},
		reload() {
			fetch('/api/harvester/reload').then(this.update());
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 10000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<div>
			<v-data-table
				:headers="headers"
				:items="plot_count"
				:loading="!loaded"
				hide-default-footer
				disable-sort
				disable-pagination
				class="elevation-2 my-2"
			>
				<template v-slot:item.ksize="{ item }">
					K{{item.ksize}}
				</template>
				
				<template v-slot:item.count="{ item }">
					<b>{{item.count}}</b>
				</template>
			</v-data-table>

			<v-data-table
				:headers="headers2"
				:items="harvester_bytes"
				:loading="!loaded"
				hide-default-footer
				disable-sort
				disable-pagination
				class="elevation-2"
			>
				<template v-slot:item.bytes="{ item }">
					<b>{{(item.bytes / Math.pow(1000, 4)).toFixed(3)}}</b> TB
				</template>
				<template v-slot:item.effective="{ item }">
					<b>{{(item.effective / Math.pow(1000, 4)).toFixed(3)}}</b> TBe
				</template>
			</v-data-table>

			<v-btn class="my-2" outlined @click="reload">{{ $t('farmer_plots.reload_plots') }}</v-btn>
		</div>
		`
})

Vue.component('farmer-plot-dirs', {
	data() {
		return {
			loaded: false,
			data: []
		}
	},
	computed: {
		headers() {
			return [
				{ text: this.$t('farmer_plot_dirs.path'), value: 'item' },
			]
		}
	},
	methods: {
		update() {
			fetch('/wapi/farmer/info')
				.then(response => response.json())
				.then(data => {
					this.data = data.plot_dirs;
					this.loaded = true;
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 10000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<v-data-table
			:headers="headers"
			:items="data"
			:loading="!loaded"
			hide-default-footer
			disable-sort
			disable-pagination
			class="elevation-2"
		>
			<template v-slot:item.item="{ item }">
				{{item}}
			</template>
		</v-data-table>
		`
})

Vue.component('farmer-blocks', {
	props: {
		limit: Number
	},
	data() {
		return {
			loaded: false,
			data: []
		}
	},
	computed: {
		headers() {
			return [
				{ text: this.$t('farmer_blocks.height'), value: 'height' },
				{ text: "TX", value: 'tx_count' },
				{ text: "K", value: 'proof.ksize' },
				{ text: this.$t('farmer_blocks.score'), value: 'proof.score' },
				{ text: this.$t('farmer_blocks.reward'), value: 'reward' },
				{ text: this.$t('farmer_blocks.tx_fees'), value: 'tx_fees' },
				{ text: this.$t('farmer_blocks.time'), value: 'time' },
			]
		}
	},
	methods: {
		update() {
			fetch('/wapi/farmer/blocks?limit=' + this.limit)
				.then(response => response.json())
				.then(data => {
					this.data = data;
					this.loaded = true;
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 30000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<v-data-table
			:headers="headers"
			:items="data"
			:loading="!loaded"
			hide-default-footer
			disable-sort
			disable-pagination
			class="elevation-2"
		>
			<template v-slot:progress>
				<v-progress-linear indeterminate absolute top></v-progress-linear>
			</template>

			<template v-slot:item.height="{ item }">
				<router-link :to="'/explore/block/height/' + item.height">{{item.height}}</router-link>
			</template>

			<template v-slot:item.reward="{ item }">
				<b>{{item.reward_amount.value}}</b> MMX
			</template>
			
			<template v-slot:item.tx_fees="{ item }">
				<b>{{item.tx_fees.value}}</b> MMX
			</template>

			<template v-slot:item.time="{ item }">
				{{new Date(item.time * 1000).toLocaleString()}}
			</template>

		</v-data-table>
		`
})

Vue.component('farmer-proofs', {
	props: {
		limit: Number
	},
	data() {
		return {
			loaded: false,
			data: []
		}
	},
	computed: {
		headers() {
			return [
				{ text: this.$t('farmer_proofs.height'), value: 'height' },
				{ text: this.$t('farmer_proofs.harvester'), value: 'harvester' },
				{ text: this.$t('farmer_proofs.score'), value: 'proof.score' },
				{ text: this.$t('farmer_proofs.sdiff'), value: 'space_diff' },
				{ text: this.$t('farmer_proofs.time'), value: 'lookup_time_ms' },
				{ text: this.$t('farmer_proofs.plot_id'), value: 'proof.plot_id' },
			]
		}
	},
	methods: {
		update() {
			fetch('/wapi/farmer/proofs?limit=' + this.limit)
				.then(response => response.json())
				.then(data => {
					this.data = data;
					this.loaded = true;
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 10000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<v-data-table
			:headers="headers"
			:items="data"
			:loading="!loaded"
			hide-default-footer
			disable-sort
			disable-pagination
			class="elevation-2"
		>
			<template v-slot:progress>
				<v-progress-linear indeterminate absolute top></v-progress-linear>
			</template>

			<template v-slot:item.height="{ item }">
				<router-link :to="'/explore/block/height/' + item.height">{{item.height}}</router-link>
			</template>	

			<template v-slot:item.lookup_time_ms="{ item }">
				{{(item.lookup_time_ms / 1000).toFixed(3)}} sec
			</template>

		</v-data-table>
		`
})

