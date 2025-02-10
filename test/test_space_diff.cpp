/*
 * test_space_diff.cpp
 *
 *  Created on: Dec 30, 2024
 *      Author: mad
 */


#include <mmx/pos/Prover.h>
#include <mmx/pos/verify.h>
#include <mmx/utils.h>

#include <vnx/vnx.h>

using namespace mmx;


int main(int argc, char** argv)
{
	mmx::secp256k1_init();

	std::map<std::string, std::string> options;
	options["f"] = "file";
	options["n"] = "iter";
	options["s"] = "diff";
	options["file"] = "plot file";
	options["iter"] = "number of iterations";
	options["diff"] = "space diff";

	vnx::write_config("log_level", 2);

	vnx::init("test_space_diff", argc, argv, options);

	int num_iter = 1000;
	int plot_filter = 4;
	std::string file_name;
	uint64_t space_diff = 1;

	vnx::read_config("file", file_name);
	vnx::read_config("iter", num_iter);
	vnx::read_config("diff", space_diff);

	const auto params = get_params();

	std::cout << "Iterations: " << num_iter << std::endl;
	std::cout << "Difficulty: " << space_diff << std::endl;

	const auto prover = std::make_shared<pos::Prover>(file_name);
	const auto ksize = prover->get_ksize();

	std::cout << "K Size: " << ksize << std::endl;
	std::cout << "Effective Size: " << get_effective_plot_size(ksize) / 1e12 << " TB" << std::endl;

	int total_proofs = 0;

	for(int iter = 0; iter < num_iter; ++iter)
	{
		const hash_t challenge(prover->get_plot_id() + std::to_string(iter));

		const auto qualities = prover->get_qualities(challenge, plot_filter);

		for(const auto& entry : qualities) {
			try {
				if(!entry.valid) {
					throw std::runtime_error(entry.error_msg);
				}
				const auto quality = pos::calc_quality(challenge, entry.meta);

				if(check_proof_threshold(params, ksize, quality, space_diff, false)) {
					total_proofs++;
				}
			}
			catch(const std::exception& ex) {
				std::cerr << "Threw: " << ex.what() << std::endl;
			}
		}
	}

	const int total_challenges = num_iter << plot_filter;

	std::cout << "---------------------------------------------------" << std::endl;
	std::cout << "Total Proofs: " << total_proofs << std::endl;
	std::cout << "Total Challenges: " << total_challenges << std::endl;
	std::cout << "Proof Chance: " << double(total_proofs) / total_challenges << std::endl;


	vnx::close();
	mmx::secp256k1_free();

	return 0;
}




