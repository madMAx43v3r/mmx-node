
app.component('farmer-info', {
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
		<div class="ui segment" v-if="data">
			<div class="ui four tiny statistics">
				<div class="statistic">
					<div class="value">{{(data.total_balance / 1e6).toFixed(2)}} MMX</div>
					<div class="label">Virtual Balance</div>
				</div>
				<div class="statistic">
					<div class="value">{{(data.total_virtual_bytes / Math.pow(1000, 4)).toFixed(3)}} TB</div>
					<div class="label">Virtual Size</div>
				</div>
				<div class="statistic">
					<div class="value">{{(data.total_bytes / Math.pow(1000, 4)).toFixed(3)}} TB</div>
					<div class="label">Physical Size</div>
				</div>
				<div class="statistic">
					<div class="value">{{((data.total_bytes + data.total_virtual_bytes) / Math.pow(1000, 4)).toFixed(3)}} TB</div>
					<div class="label">Total Farm Size</div>
				</div>
			</div>
		</div>
		`
})