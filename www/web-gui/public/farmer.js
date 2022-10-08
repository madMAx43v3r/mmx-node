
Vue.component('farmer-menu', {
	template: `
		<v-tabs>
			<v-tab to="/farmer/plots">Plots</v-tab>
			<v-tab to="/farmer/blocks">Blocks</v-tab>
			<v-tab to="/farmer/proofs">Proofs</v-tab>
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
										<div v-if="data">{{ (data.total_balance / 1e6).toFixed(2) }} MMX</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"></v-skeleton-loader>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										{{ $t('farmer_info.virtual_balance') }} 
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										<div v-if="data">{{ (data.total_virtual_bytes / Math.pow(1000, 4)).toFixed(3) }} TB</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"></v-skeleton-loader>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										{{ $t('farmer_info.virtual_size') }}
									</v-row>
								</v-col>

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
										<div v-if="data">{{ ((data.total_bytes + data.total_virtual_bytes) / Math.pow(1000, 4)).toFixed(3) }} TB</div>
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

Vue.component('farmer-plots', {
	data() {
		return {
			loaded: false,
			data: []
		}
	},
	computed: {
		headers() {
			return [
				{ text: "Type", value: 'ksize', width: "10%" },
				{ text: "Count", value: 'count' },
			]
		}
	},
	methods: {
		update() {
			fetch('/wapi/farmer/info')
				.then(response => response.json())
				.then(data => {
					this.data = [];
					for(const entry of data.plot_count) {
						this.data.push({ksize: entry[0], count: entry[1]});
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
				:items="data"
				:loading="!loaded"
				hide-default-footer
				disable-sort
				disable-pagination
				class="elevation-2"
			>
				<template v-slot:item.ksize="{ item }">
					K{{item.ksize}}
				</template>
			</v-data-table>
			<v-btn class="my-2" outlined @click="reload">Reload Plots</v-btn>
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
				{ text: "Path", value: 'item' },
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
				{ text: "Height", value: 'height' },
				{ text: "TX", value: 'tx_count' },
				{ text: "K", value: 'proof.ksize' },
				{ text: "Score", value: 'proof.score' },
				{ text: "Reward", value: 'reward' },
				{ text: "TX Fees", value: 'tx_fees' },
				{ text: "Time", value: 'time' },
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
				<b>{{item.tx_base && item.tx_base.exec_result ? item.tx_base.exec_result.total_fee_value : 0}}</b> MMX
			</template>
			
			<template v-slot:item.tx_fees="{ item }">
				<b>{{item.tx_fees / 1e6}}</b> MMX
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
				{ text: "Height", value: 'height' },
				{ text: "Harvester", value: 'harvester' },
				{ text: "Score", value: 'proof.score' },
				{ text: "Difficulty", value: 'space_diff' },
				{ text: "Time", value: 'lookup_time_ms' },
				{ text: "Plot ID", value: 'proof.plot_id' },
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

