/*
 * test_challenge_grind.cpp
 *
 *  Created on: Jan 10, 2025
 *      Author: mad
 */

#include <iostream>
#include <cstdint>
#include <cmath>

int main()
{
    const int N = 256;
    const int M = 2;					// number of choices
    const int P = 5;					// netspace / 1000
    const int num_iter = 10000;

    uint64_t total = 0;

    for(int iter = 0; iter < num_iter; ++iter)
    {
        int sum[M] = {};
        for(int k = 0; k < M; ++k) {
            for(int i = 0; i < N; ++i) {
                sum[k] += (rand() % 1000) < P ? 1 : 0;
            }
        }
        int best = 0;
        for(int k = 0; k < M; ++k) {
            best = std::max(best, sum[k]);
        }
        total += best;
    }

    const auto free_chance = pow(double(P) / 1000, M - 1);
    const auto avg_blocks = double(total) / num_iter - 0.5 * (1 - free_chance);
    const auto exp_blocks = double(N * P) / 1000;
    const auto gain = avg_blocks / exp_blocks;
    const auto effective = (gain - 1) * double(P) / 1000 * 100;

    std::cout << "free_chance = " << free_chance << std::endl;
    std::cout << "avg_blocks =  " << avg_blocks << std::endl;
    std::cout << "exp_blocks =  " << exp_blocks << std::endl;
    std::cout << "gain =        " << gain << std::endl;
    std::cout << "effective =   " << effective << " %" << std::endl;

    return 0;
}


