/*
 * mmx_posbench.cpp
 *
 *  Created on: Feb 5, 2025
 *      Author: mad
 */

#include <mmx/utils.h>
#include <mmx/pos/verify.h>
#include <vnx/vnx.h>
#include <thread>

#ifdef WITH_CUDA
#include <mmx/pos/cuda_recompute.h>
#endif


int main(int argc, char** argv)
{
	std::map<std::string, std::string> options;
	options["n"] = "iter";
	options["k"] = "ksize";
	options["r"] = "threads";
	options["v"] = "verbose";
	options["C"] = "clevel";
	options["cuda"] = "flag";
	options["devices"] = "CUDA devices";
	options["iter"] = "number of iterations";
	options["threads"] = "number of threads";

	vnx::write_config("log_level", 2);

	vnx::init("mmx_posbench", argc, argv, options);

	int num_iter = 0;
	int num_threads = 0;
	int ksize = 32;
	int clevel = 0;
	bool cuda = true;
	bool verbose = false;
	std::vector<int> cuda_devices;

	vnx::read_config("iter", num_iter);
	vnx::read_config("threads", num_threads);
	vnx::read_config("ksize", ksize);
	vnx::read_config("clevel", clevel);
	vnx::read_config("verbose", verbose);
	vnx::read_config("cuda", cuda);
	vnx::read_config("devices", cuda_devices);

	if(num_threads <= 0) {
		num_threads = std::max(std::thread::hardware_concurrency(), 4u);
	}
	if(num_iter <= 0) {
		num_iter = std::max(1 << std::max(16 - clevel, 0), 4);
	}

	std::cout << "Threads: " << num_threads << std::endl;
	std::cout << "Iterations: " << num_iter << std::endl;

#ifdef WITH_CUDA
	if(cuda) {
		std::cout << "CUDA available: yes" << std::endl;
	} else {
		std::cout << "CUDA available: disabled" << std::endl;
	}
	mmx::pos::cuda_recompute_init(cuda, cuda_devices);
	std::this_thread::sleep_for(std::chrono::milliseconds(200));	// wait for CUDA alloc
#else
	std::cout << "CUDA available: no" << std::endl;
#endif

	vnx::ThreadPool threads(num_threads, 1000);

	mmx::hash_t plot_id;
	std::vector<uint32_t> x_values;
	vnx::from_string("60C67427E27FBD77C8BDFF5EB1CC3696C08926FA12BCE8B1AE48E87099F9366B", plot_id);
	vnx::from_string("[3684070240, 4088919472, 4255854024, 2557705011, 2826775019, 784116104, 3002057429, 1544955509, 1600690361, 1551417277, 2728397286, 1771107893, 1187799433, 1044600637, 1880286022, 1921473397, 4059605908, 3921758377, 1663788388, 2187951111, 1469314847, 2221602546, 3049849478, 2483049477, 783906790, 1769349661, 627534699, 1664365119, 3108470239, 4050165503, 1719994423, 2296755013, 3689388662, 173076783, 178304083, 3395522713, 1454472817, 3194528037, 1512291786, 1987638418, 1143780087, 3120478614, 2855051906, 693584283, 3818941816, 4127002689, 2989697755, 3891407791, 2509655266, 4258376726, 1843252036, 3191219821, 2088352375, 213252018, 4115819364, 3175076101, 3561808706, 1948511669, 3322128563, 3750559212, 2272209440, 1293941526, 437177217, 2820197201, 3807400022, 7496129, 2683510002, 3807839482, 2233243744, 2411092355, 1331204979, 2038088672, 2167782813, 3830146105, 537456114, 2401528407, 2652250456, 2221908904, 890566783, 708924513, 2596290458, 340917615, 3250050811, 3771386335, 3494357838, 1179822413, 2341748577, 602106011, 1122065619, 252962133, 278550961, 1768804869, 669497081, 1990086308, 2491380917, 51625349, 4207300886, 3095591768, 1852131389, 609642249, 2918683512, 2059312217, 3335572914, 2736167997, 1528047374, 4124848408, 902683345, 4263117025, 108772979, 485864815, 2410357795, 908723453, 1183430568, 2815414658, 2737238764, 2669408162, 2850938826, 2890536155, 491707862, 2553723643, 1034532861, 3214153497, 3097594346, 2701020101, 678153046, 2932267943, 1365864923, 532310940, 2351720145, 4080824906, 2893128375, 2595930727, 1064911548, 3834810248, 3565525092, 163085774, 685730942, 2810511962, 2540444228, 1857924416, 4174369771, 2145288036, 587439552, 718732787, 1169724691, 3254817017, 3946843084, 954721517, 3373230078, 3637676521, 1331632982, 4086478124, 4116294100, 4120279824, 476981752, 2430423022, 220679215, 4117454286, 4166904409, 2171504903, 2001727739, 3383221100, 2596677253, 988991329, 2638232513, 1095569023, 3303068778, 1931231583, 2226576220, 1480273400, 3911488963, 1608929138, 1376754282, 61629307, 3957255986, 514129543, 2169952004, 3308565016, 3572656193, 1573653323, 961047589, 2310528924, 2099635716, 817068327, 3653864895, 4168803514, 2147588334, 2794673138, 492271828, 3268899536, 1623851505, 1985375222, 396546240, 3609981176, 2079278511, 2161006185, 2007577134, 2954139926, 1277553298, 3205868414, 4257573912, 3376256719, 4227206123, 585150605, 2904436845, 2619013042, 3125718317, 3035845111, 2049651560, 161957643, 393761167, 2783560380, 1211203226, 3313590097, 479870197, 830535414, 3613172362, 3953259697, 265953611, 1493207470, 955279780, 2133758876, 2884678190, 3234737433, 1365475005, 2908229202, 2072744826, 4263640821, 2236131683, 894009793, 307201858, 3291293763, 2184230914, 776021200, 867100221, 4032840254, 2454442006, 1289389606, 1247346023, 386849697, 1757849095, 973341783, 3958516371, 1277136670, 205471439, 2514111789, 1777386820, 1436581469, 1494466641, 512070215, 2845825157, 329403080, 1115243938, 4009264613, 3806285763, 3170487941]", x_values);

	for(auto& x : x_values) {
		x >>= clevel;
	}

	std::atomic<int64_t> progress {0};
	const auto time_begin = mmx::get_time_ms();

	for(int i = 0; i < num_iter; ++i)
	{
		threads.add_task([&]() {
			if(!vnx::do_run()) {
				return;
			}
			std::vector<uint32_t> x_out;
			const auto entries = mmx::pos::compute(x_values, &x_out, plot_id, 32, clevel);
			if(entries.empty() || x_out.size() != entries.size() * 256) {
				throw std::logic_error("invalid proof");
			}
			if((progress++) % std::max((num_iter / 100), 1) == 0) {
				std::cout << ".";
				std::cout.flush();
			}
		});
	}

	threads.sync();

	const auto elapsed_sec = (mmx::get_time_ms() - time_begin) / 1e3;

	std::cout << std::endl;
	if(verbose) {
		std::cout << "Took " << elapsed_sec << " sec" << std::endl;
	}
	const double proof_time = elapsed_sec / num_iter;
	std::cout << "Proof time: " << proof_time << " sec (average)" << std::endl;

	const double max_plots_ssd = 8.0 / proof_time;
	const double max_plots_hdd = max_plots_ssd * 1024;
	const double saved_per_level = 1.0 * pow(1024, 3) / pow(1000, 3) / pow(2, 32 - ksize);	// GB
	const double plot_size = mmx::get_effective_plot_size(ksize) / pow(1000, 3);
	const double plot_size_hdd = plot_size - saved_per_level * clevel;
	const double plot_size_ssd = plot_size / 2.5 - saved_per_level * clevel;

	std::cout << "Plot: k" << ksize << " C" << clevel << std::endl;
	std::cout << "Plot Size (SSD): " << plot_size_ssd << " GB" << std::endl;
	std::cout << "Plot Size (HDD): " << plot_size_hdd << " GB" << std::endl;
	std::cout << "Max Farm Size (SSD): " << max_plots_ssd * plot_size_ssd / pow(1000, 2) << " PB" << std::endl;
	std::cout << "Max Farm Size (HDD): " << max_plots_hdd * plot_size_hdd / pow(1000, 2) << " PB" << std::endl;

#ifdef WITH_CUDA
	mmx::pos::cuda_recompute_shutdown();
#endif

	vnx::close();

	return 0;
}









