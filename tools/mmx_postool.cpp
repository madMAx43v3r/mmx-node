/*
 * mmx_postool.cpp
 *
 *  Created on: Apr 5, 2024
 *      Author: mad
 */

#include <mmx/pos/Prover.h>
#include <mmx/pos/verify.h>

#include <vnx/vnx.h>
#include <cmath>
#include <thread>

using namespace mmx;


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

	std::map<std::string, std::string> options;
	options["f"] = "file";
	options["n"] = "iter";
	options["r"] = "threads";
	options["v"] = "verbose";
	options["D"] = "verbose-debug";
	options["file"] = "plot files";
	options["iter"] = "number of iterations";
	options["threads"] = "number of threads";

	vnx::write_config("log_level", 2);

	vnx::init("mmx_postool", argc, argv, options);

	bool debug = false;
	bool verbose = false;
	int num_iter = 10;

	const auto processor_count = std::thread::hardware_concurrency();
	int num_threads = processor_count ? processor_count : 16;
	int plot_filter = 4;
	std::vector<std::string> file_names;

	vnx::read_config("verbose-debug", debug);
	vnx::read_config("verbose", verbose);
	vnx::read_config("file", file_names);
	vnx::read_config("iter", num_iter);
	vnx::read_config("threads", num_threads);

	if(debug) {
		verbose = true;
	}
	if(verbose) {
		std::cout << "Threads: " << num_threads << std::endl;
		std::cout << "Iterations: " << num_iter << std::endl;
	}
	vnx::ThreadPool threads(num_threads, 10);

	struct summary_t {
		std::string file;
		bool valid = false;
		std::atomic<uint32_t> num_pass {0};
		std::atomic<uint32_t> num_fail {0};
	};

	std::mutex mutex;
	std::vector<std::shared_ptr<summary_t>> result;

	for(const auto& file_name : file_names)
	{
		auto out = std::make_shared<summary_t>();
		out->file = file_name;
		try {
			auto prover = std::make_shared<pos::Prover>(file_name);
			prover->debug = debug;

			std::cout << "--------------------------------------------------------------------------------" << std::endl;
			std::cout << "Checking '" << file_name << "'" << std::endl;

			auto header = prover->get_header();
			if(verbose) {
				std::cout << "Size: " << (header->has_meta ? "HDD" : "SSD") << " K" << header->ksize << " C" << prover->get_clevel()
						<< " (" << header->plot_size / pow(1024, 3) << " GiB)" << std::endl;
				std::cout << "Plot ID: " << prover->get_plot_id().to_string() << std::endl;
				std::cout << "Farmer Key: " << header->farmer_key.to_string() << std::endl;
				std::cout << "Contract: " << (header->contract ? header->contract->to_string() : std::string("N/A")) << std::endl;
			}
			out->valid = true;

			for(int iter = 0; iter < num_iter && vnx::do_run(); ++iter)
			{
				threads.add_task([iter, prover, header, out, plot_filter, verbose, debug, &mutex]() {
					const hash_t challenge(std::to_string(iter));
					try {
						const auto qualities = prover->get_qualities(challenge, plot_filter);

						for(const auto& entry : qualities)
						{
							if(!entry.valid) {
								std::lock_guard<std::mutex> lock(mutex);
								std::cerr << "Threw: " << entry.error_msg << std::endl;
								out->num_fail++;
								continue;
							}
							if(debug) {
								std::lock_guard<std::mutex> lock(mutex);
								std::cout << "[" << iter << "] index = " << entry.index << ", quality = " << entry.quality.to_string() << std::endl;
							}
							try {
								std::vector<uint32_t> proof;
								if(entry.proof.size()) {
									proof = entry.proof;
								} else {
									proof = prover->get_full_proof(challenge, entry.index).proof;
								}
								const auto quality = pos::verify(proof, challenge, header->plot_id, plot_filter, header->ksize);
								if(quality != entry.quality) {
									throw std::logic_error("invalid quality");
								}
								if(debug) {
									std::lock_guard<std::mutex> lock(mutex);
									std::cout << "Proof " << entry.index << " passed: ";
									for(const auto X : proof) {
										std::cout << X << " ";
									}
									std::cout << std::endl;
								}
								out->num_pass++;
							}
							catch(const std::exception& ex) {
								std::lock_guard<std::mutex> lock(mutex);
								std::cerr << "Threw: " << ex.what() << std::endl;
								out->num_fail++;
							}
						}
					}
					catch(const std::exception& ex) {
						std::lock_guard<std::mutex> lock(mutex);
						std::cerr << "Threw: " << ex.what() << std::endl;
						out->num_fail++;
					}
				});
			}
			threads.sync();

			const auto expected = uint64_t(num_iter) << plot_filter;
			std::cout << "Pass: " << out->num_pass << " / " << expected << ", " << float(100 * out->num_pass) / expected << " %" << std::endl;
			std::cout << "Fail: " << out->num_fail << " / " << expected << ", " << float(100 * out->num_fail) / expected << " %" << std::endl;

			if(!vnx::do_run()) {
				break;
			}
		}
		catch(const std::exception& ex) {
			std::cerr << "--------------------------------------------------------------------------------" << std::endl;
			std::cerr << "Failed to open plot " << file_name << ": " << ex.what() << std::endl;
		}
		result.push_back(out);
	}

	std::vector<std::string> bad_plots;
	for(auto entry : result) {
		if(entry->num_fail > 1) {
			bad_plots.push_back(entry->file);
		}
	}

	std::cout << "--------------------------------------------------------------------------------" << std::endl;
	std::cout << "Bad plots: ";
	if(bad_plots.empty()) {
		std::cout << "None" << std::endl;
	} else {
		std::cout << bad_plots.size() << std::endl;
		for(auto file : bad_plots) {
			std::cout << file << std::endl;
		}
	}

	threads.close();
	vnx::close();
	mmx::secp256k1_free();

	return bad_plots.size() ? -1 : 0;
}


