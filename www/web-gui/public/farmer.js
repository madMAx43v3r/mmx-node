
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

		<div v-if="data">
			<v-row align="center" justify="space-around" class="my-2">
				<v-col cols="12">			
					<v-card>					
						<v-card-title>
							<v-row align="center" justify="space-around">

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">
									<v-row align="center" justify="space-around">
										{{ (data.total_balance / 1e6).toFixed(2) }} MMX
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										Virtual Balance
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										{{ (data.total_virtual_bytes / Math.pow(1000, 4)).toFixed(3) }} TB
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										Virtual Size
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										{{ (data.total_bytes / Math.pow(1000, 4)).toFixed(3) }} TB
									</v-row>
									<v-row align="center" justify="space-around" class="subtitle-1">
										Physical Size
									</v-row>
								</v-col>

								<v-col cols="12" xl="3" md="3" sm="6" class="text-center my-2">					
									<v-row align="center" justify="space-around">
										{{ ((data.total_bytes + data.total_virtual_bytes) / Math.pow(1000, 4)).toFixed(3) }} TB
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