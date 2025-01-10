/*
 * test_challenge_grind.cpp
 *
 *  Created on: Jan 10, 2025
 *      Author: mad
 */

#include <iostream>
#include <cstdint>
#include <cmath>
#include <random>

int main()
{
    const int N = 5;
    const int M = 2;					// number of choices
    const int P = 500;					// netspace / 1000
    const int num_iter = 10000;

    const auto free_chance = pow(double(P) / 1000, M - 1);

    std::default_random_engine engine;
    std::uniform_real_distribution<double> dist(0, 1);

    uint64_t total = 0;
    uint64_t lost = 0;

    for(int iter = 0; iter < num_iter; ++iter)
    {
        int sum[M] = {};
        for(int k = 0; k < M; ++k) {
            for(int i = 0; i < N; ++i) {
                sum[k] += (engine() % 1000) < P ? 1 : 0;
            }
        }
        int best = 0;
        int index = 0;
        for(int k = 0; k < M; ++k) {
            if(sum[k] > best) {
                best = sum[k];
                index = k;
            }
        }
        total += best;

        if(index > 0) {
            if(dist(engine) > free_chance) {
                lost++;
            }
        }
    }

    const auto loss_chance = double(lost) / num_iter;
    const auto avg_blocks = double(total) / num_iter - loss_chance;
    const auto exp_blocks = double(N * P) / 1000;
    const auto gain = avg_blocks / exp_blocks;
    const auto effective = (gain - 1) * double(P) / 1000 / 256 * 100;

    std::cout << "free_chance = " << free_chance << std::endl;
    std::cout << "loss_chance = " << loss_chance << std::endl;
    std::cout << "avg_blocks =  " << avg_blocks << std::endl;
    std::cout << "exp_blocks =  " << exp_blocks << std::endl;
    std::cout << "gain =        " << gain << std::endl;
    std::cout << "effective =   " << effective << " %" << std::endl;

    return 0;
}


