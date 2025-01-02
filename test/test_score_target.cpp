/*
 * test_score_target.cpp
 *
 *  Created on: Dec 27, 2024
 *      Author: mad
 */

#include <iostream>
#include <cstdint>

int main(int argc, char** argv)
{
	const int N = argc > 1 ? std::atoi(argv[1]) : 1;
    const int K = 10000000;

    int64_t sum = 0;
    for(int i = 0; i < K; ++i)
    {
        int tmp = 1 << 16;
        for(int k = 0; k < N; ++k) {
            tmp = std::min(tmp, rand() % (1 << 16));
        }
        sum += tmp;
    }

    std::cout << sum / K << std::endl;

    return 0;
}


