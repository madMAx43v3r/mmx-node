
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
	// methods: {
	// 	update() {
	// 		fetch('/wapi/node/info')
	// 			.then(response => response.json())
	// 			.then(data => this.data = data);
	// 	}
	// },
	// created() {
	// 	this.update();
	// 	this.timer = setInterval(() => { this.update(); }, 5000);
	// },
	// beforeDestroy() {
	// 	clearInterval(this.timer);
	// },
	computed: {
		nodeInfo() {
			return this.$root.nodeInfo;
		}
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
										<div v-if="nodeInfo">{{ nodeInfo.is_synced ? $t('common.yes') : $t('common.no') }}</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"/>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										{{ $t('node_info.synced') }}
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around" width=100>									
										<div v-if="nodeInfo">{{ nodeInfo.height }}</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"/>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										{{ $t('node_info.height') }}
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										<div v-if="nodeInfo">{{ (nodeInfo.total_space / Math.pow(1000, 5)).toFixed(3) }} PB</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"/>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										{{ $t('node_info.netspace') }}
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										<div v-if="nodeInfo">{{ (nodeInfo.total_supply / Math.pow(10, 6)).toFixed(0) }} MMX</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"/>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										{{ $t('node_info.supply') }}
									</v-row>
								</v-col>

							</v-row>
						</v-card-title>
					</v-card>
				</v-col>
			</v-row>
			
			<v-row class="my-2 py-0">
				<v-col cols="12" class="my-0 py-0">			
					<v-card min-height="90">					
						<v-card-title>
							<v-row align="center" justify="space-around">

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">
									<v-row align="center" justify="space-around">
										<div v-if="nodeInfo">{{ nodeInfo.address_count }}</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"/>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										{{ $t('node_info.no_addresses') }}
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										<div v-if="nodeInfo">{{ (nodeInfo.block_size * 100).toFixed(2) }} %</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"/>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										{{ $t('node_info.block_size') }}
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										<div v-if="nodeInfo">{{ (nodeInfo.time_diff / 10 / Math.pow(10, 3)).toFixed(3) }} MH/s</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"/>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										{{ $t('node_info.vdf_speed') }}
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										<div v-if="nodeInfo">{{ (nodeInfo.block_reward.value).toFixed(3) }} MMX</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"/>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										{{ $t('node_info.block_reward') }}
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

Vue.component('node-peers', {
	data() {
		return {
			data: [],
			timer: null,
			loaded: false
		}
	},
	computed: {
		headers() {
			return [
				{ text: this.$t('node_peers.ip'), value: 'address', width: "5%" },
				{ text: this.$t('node_peers.height'), value: 'height', width: "5%" },
				{ text: this.$t('node_peers.type'), value: 'type', width: "5%"},
				{ text: this.$t('node_peers.version'), value: 'version', width: "5%" },
				{ text: this.$t('node_peers.received'), value: 'received', width: "10%" },
				{ text: this.$t('node_peers.send'), value: 'send', width: "10%" },
				{ text: this.$t('node_peers.ping'), value: 'ping', width: "10%" },
				{ text: this.$t('node_peers.duration'), value: 'duration', width: "10%" },
				{ text: this.$t('node_peers.credits'), value: 'credits', width: "10%" },
				{ text: this.$t('node_peers.connection'), value: 'connection', width: "10%" },
			]
		}
	},
	methods: {
		update() {
			fetch('/api/router/get_peer_info')
				.then(response => response.json())
				.then(data => {
					this.data = data; 
					this.loaded = true;
				});
		}
	},
	created() {
		this.update();
		this.timer = setInterval(() => { this.update(); }, 5000);
	},
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<v-data-table
			:headers="headers"
			:items="data.peers"
			:loading="!loaded"
			hide-default-footer
			disable-sort
			disable-pagination
			class="elevation-2"
		>
			<template v-slot:item.height="{ item }">
				{{item.is_synced ? "" : "!"}}{{item.height}}
			</template>
			
			<template v-slot:item.version="{ item }">
				{{(item.version / 100).toFixed()}}.{{item.version % 100}}
			</template>
			
			<template v-slot:item.received="{ item }">
				<b>{{(item.bytes_recv / item.connect_time_ms / 1.024).toFixed(1)}}</b> KB/s
			</template>
			
			<template v-slot:item.send="{ item }">
				<b>{{(item.bytes_send / item.connect_time_ms / 1.024).toFixed(1)}}</b> KB/s
			</template>	
			
			<template v-slot:item.ping="{ item }">
				<b>{{item.ping_ms}}</b> ms
			</template>

			<template v-slot:item.duration="{ item }">
				<b>{{(item.connect_time_ms / 1000 / 60).toFixed()}}</b> min
			</template>			

			<template v-slot:item.connection="{ item }">
				{{item.is_outbound ? $t('node_peers.outbound') : $t('node_peers.inbound') }}
			</template>				
		</v-data-table>	

		<table class="ui compact definition table striped" v-if="data">

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
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<div>
			<v-card>
				<v-skeleton-loader type="image@3" height="482" v-if="!data && loading"/>
				<v-fade-transition>					
					<v-card-text v-if="data">
						<vue-plotly :data="data" :layout="layout" :display-mode-bar="false"></vue-plotly>
					</v-card-text>
				</v-fade-transition>
			</v-card>
		</div>
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
				title: this.$t('vdf_speed_graph.title', ['MH/s']),
				xaxis: {
					title: this.$t('common.height')
				},
				yaxis: {
					title: "MH/s"
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
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<div>
			<v-card>
				<v-skeleton-loader type="image@3" height="482" v-if="!data && loading"/>
				<v-fade-transition>					
					<v-card-text v-if="data">
						<vue-plotly :data="data" :layout="layout" :display-mode-bar="false"></vue-plotly>
					</v-card-text>
				</v-fade-transition>
			</v-card>
		</div>
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
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<div>

			<v-card>
				<v-skeleton-loader type="image@3" height="482" v-if="!base_data && loading"/>
				<v-fade-transition>					
					<v-card-text v-if="base_data">
						<vue-plotly :data="base_data" :layout="base_layout" :display-mode-bar="false"></vue-plotly>
					</v-card-text>
				</v-fade-transition>
			</v-card>				
		
			<v-card class="my-2">
				<v-skeleton-loader type="image@3" height="482" v-if="!fee_data && loading"/>
				<v-fade-transition>
					<v-card-text v-if="fee_data">
						<vue-plotly :data="fee_data" :layout="fee_layout" :display-mode-bar="false"></vue-plotly>
					</v-card-text>
				</v-fade-transition>
			</v-card>
			
		</div>
		`
})

Vue.component('node-log', {
	data() {
		return {
			limit: 100,
			level: 3,
			module: null,
			currentItem: 0
		}
	},
	template: `
		<div>
			<v-btn-toggle
				v-model="currentItem"
				dense
				class="mb-2"
			>
				<v-btn @click="module = null">{{ $t('node_log.terminal') }}</v-btn>
				<v-btn @click="module = 'Node'">{{ $t('node_log.node') }}</v-btn>
				<v-btn @click="module = 'Router'">{{ $t('node_log.router') }}</v-btn>
				<v-btn @click="module = 'Wallet'">{{ $t('node_log.wallet') }}</v-btn>
				<v-btn @click="module = 'Farmer'">{{ $t('node_log.farmer') }}</v-btn>
				<v-btn @click="module = 'Harvester'">{{ $t('node_log.harvester') }}</v-btn>
				<v-btn @click="module = 'TimeLord'">{{ $t('node_log.timelord') }}</v-btn>
			</v-btn-toggle>
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
			data: [],
			timer: null,
			loaded: false
		}
	},
	computed: {
		headers() {
			return [
				{ text: this.$t('node_log_table.time'), value: 'time', align: 'left', width: "10%"},
				{ text: this.$t('node_log_table.module'), value: 'module', width: "10%"},
				{ text: this.$t('node_log_table.message'), value: 'message' },
			]
		}
	},
	methods: {
		update() {
			fetch('/wapi/node/log?limit=' + this.limit + "&level=" + this.level + (this.module ? "&module=" + this.module : ""))
				.then(response => response.json())
				.then(data => {
					this.data = data;
					if(this.$isWinGUI) 
					{
						this.data = this.data.filter( item => item.module != 'HttpServer');
					}
					this.loaded = true;
				});
		},
		itemClass(item) {
			var result = "";

			switch(item.level) {
				case 1:
					result = 'error';
					break;
				case 2:
					result = 'warning';
					break;
				default:
					result = '';
			}

			return result;
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
	beforeDestroy() {
		clearInterval(this.timer);
	},
	template: `
		<div>
			<v-data-table
				:headers="headers"
				:items="data"
				:loading="!loaded"
				:item-class="itemClass"
				hide-default-footer
				disable-sort
				disable-pagination
				dense
				class="elevation-2"
			>
				<template v-slot:progress>
					<v-progress-linear indeterminate absolute top></v-progress-linear>
					<v-skeleton-loader type="table-row-divider@6" />
				</template>

				<template v-slot:item.time="{ item }">
					{{ new Date(item.time / 1000).toLocaleTimeString() }}
				</template>

			</v-data-table>
		</div>
		`
})

