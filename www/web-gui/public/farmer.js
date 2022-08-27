
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
										Virtual Balance
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										<div v-if="data">{{ (data.total_virtual_bytes / Math.pow(1000, 4)).toFixed(3) }} TB</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"></v-skeleton-loader>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										Virtual Size
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										<div v-if="data">{{ (data.total_bytes / Math.pow(1000, 4)).toFixed(3) }} TB</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"></v-skeleton-loader>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										Physical Size
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										<div v-if="data">{{ ((data.total_bytes + data.total_virtual_bytes) / Math.pow(1000, 4)).toFixed(3) }} TB</div>
										<v-skeleton-loader v-else type="heading" width="50%" align="center"></v-skeleton-loader>
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										Total Farm Size
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