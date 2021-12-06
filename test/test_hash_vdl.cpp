/*
 * test_hash_vdl.cpp
 *
 *  Created on: Dec 6, 2021
 *      Author: mad
 */

#include <mmx/ProofOfTime.hxx>

#include <vnx/vnx.h>

using namespace mmx;


int main(int argc, char** argv)
{
	const size_t num_iter = 5000 * 1000;
	const size_t segment_len = num_iter / 256;

	const auto vdl_begin = vnx::get_time_micros();

	const hash_t begin("test");
	std::vector<time_segment_t> proof;
	{
		size_t j = 0;
		hash_t point = begin;
		for(size_t i = 0; i < num_iter; ++i)
		{
			if(i > 0 && i % segment_len == 0)
			{
				time_segment_t seg;
				seg.num_iters = i - j;
				seg.output = point;
				proof.push_back(seg);
				j = i;
			}
			point = hash_t(point.bytes);
		}
		if(num_iter > j)
		{
			time_segment_t seg;
			seg.num_iters = num_iter - j;
			seg.output = point;
			proof.push_back(seg);
		}
	}
	std::cout << "VDL took " << (vnx::get_time_micros() - vdl_begin) / 1000 << " ms" << std::endl;

	const auto verify_begin = vnx::get_time_micros();

#pragma omp parallel for
	for(size_t i = 0; i < proof.size(); ++i)
	{
		hash_t point;
		if(i > 0) {
			point = proof[i - 1].output;
		} else {
			point = begin;
		}
		for(size_t k = 0; k < proof[i].num_iters; ++k) {
			point = hash_t(point.bytes);
		}
		if(point != proof[i].output) {
			throw std::logic_error("invalid proof at segment " + std::to_string(i));
		}
	}

	std::cout << "Verify took " << (vnx::get_time_micros() - verify_begin) / 1000 << " ms" << std::endl;

	return 0;
}
