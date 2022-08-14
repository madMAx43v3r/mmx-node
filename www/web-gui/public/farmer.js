
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
	unmounted() {
		clearInterval(this.timer);
	},
	template: `

		<div v-if="data">
		<v-container fluid id="features" class="pt-0">

			<v-col cols="12">
				<v-row align="center" justify="space-around">
					<v-col cols="12" xl="3" md="3" sm="6" class="text-center">
						<v-card>
							<v-card-title>{{(data.total_balance / 1e6).toFixed(2)}} MMX</v-card-title>
							<v-card-text>Virtual Balance</v-card-text>
						</v-card>
					</v-col>
					<v-col cols="12" xl="3" md="3" sm="6" class="text-center">					
						<v-card>
							<v-card-title>{{(data.total_virtual_bytes / Math.pow(1000, 4)).toFixed(3)}} TB</v-card-title>
							<v-card-text>Virtual Size</v-card-text>
						</v-card>
					</v-col>
					<v-col cols="12" xl="3" md="3" sm="6" class="text-center">	
						<v-card>
							<v-card-title>{{(data.total_bytes / Math.pow(1000, 4)).toFixed(3)}} TB</v-card-title>
							<v-card-text>Physical Size</v-card-text>
						</v-card>
					</v-col>
					<v-col cols="12" xl="3" md="3" sm="6" class="text-center">
						<v-card>
							<v-card-title>{{((data.total_bytes + data.total_virtual_bytes) / Math.pow(1000, 4)).toFixed(3)}} TB</v-card-title>
							<v-card-text>Total Farm Size</v-card-text>
						</v-card>
					</v-col>
				</v-row>

			</v-col>

		</v-container>
		</div>
		`
})