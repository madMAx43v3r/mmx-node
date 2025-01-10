/*
 * test_proof_grind.cpp
 *
 *  Created on: Jan 10, 2025
 *      Author: mad
 */

#include <iostream>
#include <cstdint>
#include <random>

int main()
{
    const int N = 256;
    const int M = 2;					// number of choices
    const int P = 4;					// avg num proofs
    const int num_iter = 10000;

    std::default_random_engine engine;
    std::uniform_real_distribution<double> dist(0, 2 * P);

    uint64_t total = 0;
    uint64_t expected = 0;

    for(int iter = 0; iter < num_iter; ++iter)
    {
        int sum[M] = {};
        for(int k = 0; k < M; ++k) {
            for(int i = 0; i < N; ++i) {
                sum[k] += dist(engine) + 0.5;
            }
        }
        int best = 0;
        for(int k = 0; k < M; ++k) {
            best = std::max(best, sum[k]);
            expected += sum[k];
        }
        total += best;
    }

    const auto avg_proofs = double(total) / num_iter;
    const auto exp_proofs = double(expected) / M / num_iter;

    std::cout << "avg_proofs = " << avg_proofs << std::endl;
    std::cout << "exp_proofs = " << exp_proofs << std::endl;

   	std::cout << "gain = " << avg_proofs / exp_proofs << std::endl;

    return 0;
}


