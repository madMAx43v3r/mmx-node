/*
 * cuda_recompute.h
 *
 *  Created on: Feb 2, 2025
 *      Author: mad
 */

#ifndef INCLUDE_MMX_POS_CUDA_RECOMPUTE_H_
#define INCLUDE_MMX_POS_CUDA_RECOMPUTE_H_

#include <mmx/hash_t.hpp>
#include <mmx/pos/config.h>

#include <cstdint>
#include <vector>
#include <string>
#include <set>
#include <memory>


namespace mmx {
namespace pos {

struct cuda_device_t {
	int index = -1;
	std::string name;
	uint32_t max_resident = 0;
	uint64_t buffer_size = 0;
};

struct cuda_result_t {
	uint64_t id = 0;
	bool failed = false;
	std::string error;
	std::vector<uint32_t> X;
	std::vector<std::pair<uint32_t, bytes_t<META_BYTES_OUT>>> entries;
};

bool have_cuda_recompute();

std::vector<cuda_device_t> get_cuda_devices();

std::vector<cuda_device_t> get_cuda_devices_used();

void cuda_recompute_init(const bool enable = true, const std::vector<int>& device_list = {});

void cuda_recompute_shutdown();

uint64_t cuda_recompute(const int ksize, const int xbits, const hash_t& plot_id, const std::vector<uint32_t>& x_values);

std::shared_ptr<const cuda_result_t> cuda_recompute_poll(const std::set<uint64_t>& jobs);


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_CUDA_RECOMPUTE_H_ */
