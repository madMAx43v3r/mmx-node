/*
 * mem_hash.cu
 *
 *  Created on: Feb 2, 2025
 *      Author: mad
 */

#include <mmx/pos/cuda_recompute.h>

#include <cuda_sha512.h>
#include <cuda_runtime.h>

#include <mutex>
#include <tuple>
#include <queue>
#include <atomic>
#include <stdexcept>
#include <algorithm>
#include <condition_variable>


__device__ inline
uint32_t cuda_rotl_32(const uint32_t w, const uint32_t c) {
	return __funnelshift_l(w, w, c);
}

#define MMXPOS_HASHROUND(a, b, c, d) \
	a = a + b;              \
	d = cuda_rotl_32(d ^ a, 16); \
	c = c + d;              \
	b = cuda_rotl_32(b ^ c, 12); \
	a = a + b;              \
	d = cuda_rotl_32(d ^ a, 8);  \
	c = c + d;              \
	b = cuda_rotl_32(b ^ c, 7);


__device__
static const uint32_t MEM_HASH_INIT[16] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
};

__global__
void cuda_gen_mem_array(uint4* mem_out, uint4* key_out, const uint32_t* id, const uint32_t mem_size, const uint32_t x_0)
{
	const uint32_t x = blockIdx.x * blockDim.x + threadIdx.x;
	const uint32_t num_entries = gridDim.x * blockDim.x;

	__align__(8) uint32_t msg[32] = {};

	msg[0] = x_0 + x;

	for(int i = 0; i < 8; ++i) {
		msg[1 + i] = id[i];
	}
	__align__(8) uint32_t key[16] = {};

	cuda_sha512((uint64_t*)msg, 4 + 32, (uint64_t*)key);

	for(int i = 0; i < 4; ++i) {
		key_out[x * 4 + i] = make_uint4(key[i * 4 + 0], key[i * 4 + 1], key[i * 4 + 2], key[i * 4 + 3]);
	}

	uint32_t state[32];
	for(int i = 0; i < 16; ++i) {
		state[i] = key[i];
	}
	for(int i = 0; i < 16; ++i) {
		state[16 + i] = MEM_HASH_INIT[i];
	}

	uint32_t b = 0;
	uint32_t c = 0;

	for(uint32_t i = 0; i < mem_size / 32; ++i)
	{
		for(int j = 0; j < 4; ++j) {
#pragma unroll
			for(int k = 0; k < 16; ++k) {
				MMXPOS_HASHROUND(state[k], b, c, state[16 + k]);
			}
		}

#pragma unroll
		for(int k = 0; k < 8; ++k) {
			mem_out[(uint64_t(i) * num_entries + x) * 8 + k] =
					make_uint4(state[k * 4 + 0], state[k * 4 + 1], state[k * 4 + 2], state[k * 4 + 3]);
		}
	}
}

__global__
void cuda_calc_mem_hash(uint32_t* mem, uint32_t* hash, const int num_iter)
{
	const uint32_t x = threadIdx.x;
	const uint32_t k = threadIdx.y;
	const uint32_t y = (blockIdx.z * gridDim.y + blockIdx.y) * blockDim.y + threadIdx.y;
	const uint32_t num_entries = (gridDim.z * blockDim.z) * (gridDim.y * blockDim.y);

	static constexpr int N = 32;

	__shared__ uint32_t lmem[4][N*N];

	for(int i = 0; i < N; ++i) {
		lmem[k][i * N + x] = mem[(uint64_t(i) * num_entries + y) * N + x];
	}
	__syncwarp();

	uint32_t state = lmem[k][(N - 1) * N + x];

	__syncwarp();

	for(int iter = 0; iter < num_iter; ++iter)
	{
		uint32_t sum = cuda_rotl_32(state, x % 32);

		for(int offset = 16; offset > 0; offset /= 2) {
			sum += __shfl_down_sync(0xFFFFFFFF, sum, offset);
		}
		uint32_t dir = 0;
		if(x == 0) {
			dir = sum + (sum << 11) + (sum << 22);
		}
		sum = __shfl_sync(0xFFFFFFFF, sum, 0);
		dir = __shfl_sync(0xFFFFFFFF, dir, 0);

		const uint32_t bits = (dir >> 22) % 32u;
		const uint32_t offset = (dir >> 27);

		state += cuda_rotl_32(lmem[k][offset * N + (iter + x) % N], bits) ^ sum;

		__syncwarp();

		atomicXor(&lmem[k][offset * N + x], state);

		__syncwarp();
	}

	hash[y * N + x] = state;
}

__global__
void cuda_final_mem_hash(uint4* hash_out, uint32_t* Y_out, const uint4* mem_hash, const uint4* key_in, const uint32_t KMASK)
{
	const uint32_t x = blockIdx.x * blockDim.x + threadIdx.x;

	__align__(8) uint32_t msg[64] = {};

	for(int i = 0; i < 4; ++i) {
		const auto tmp = key_in[x * 4 + i];
		msg[i * 4 + 0] = tmp.x;
		msg[i * 4 + 1] = tmp.y;
		msg[i * 4 + 2] = tmp.z;
		msg[i * 4 + 3] = tmp.w;
	}
	for(int i = 0; i < 8; ++i) {
		const auto tmp = mem_hash[x * 8 + i];
		msg[16 + i * 4 + 0] = tmp.x;
		msg[16 + i * 4 + 1] = tmp.y;
		msg[16 + i * 4 + 2] = tmp.z;
		msg[16 + i * 4 + 3] = tmp.w;
	}
	__align__(8) uint32_t hash[16] = {};

	cuda_sha512((uint64_t*)msg, 64 + 128, (uint64_t*)hash);

	uint32_t Y_i = 0;
	for(int i = 0; i < mmx::pos::N_META; ++i) {
		Y_i = Y_i ^ hash[i];
		hash[i] = hash[i] & KMASK;
	}
	for(int i = mmx::pos::N_META; i < 16; ++i) {
		hash[i] = 0;
	}
	Y_i &= KMASK;

	for(int i = 0; i < 4; ++i) {
		hash_out[x * 4 + i] = make_uint4(hash[i * 4 + 0], hash[i * 4 + 1], hash[i * 4 + 2], hash[i * 4 + 3]);
	}
	Y_out[x] = Y_i;
}


namespace mmx {
namespace pos {

struct device_t {

	int index = -1;
	uint32_t max_ids = 0;
	uint64_t buffer_size = 0;

	uint32_t* id_dev = nullptr;
	uint32_t* hash_dev = nullptr;
	uint32_t* key_dev = nullptr;
	uint32_t* M_dev = nullptr;
	uint32_t* Y_dev = nullptr;

	uint32_t* M_buf = nullptr;
	uint32_t* Y_buf = nullptr;

};

struct request_t {

	uint64_t id = 0;
	int ksize = 0;
	int xbits = 0;
	hash_t plot_id;
	std::vector<uint32_t> x_values;

	std::vector<uint32_t> X_tmp;
	std::vector<uint32_t> Y_tmp;
	std::vector<std::array<uint32_t, N_META>> M_tmp;

};

std::mutex g_mutex;
std::condition_variable g_signal;
std::atomic<bool> have_init {false};
std::vector<cuda_device_t> g_device_list;

std::atomic<uint64_t> next_request_id {1};
std::vector<std::tuple<int, int>> g_order_queue;
std::map<std::tuple<int, int>, std::queue<std::shared_ptr<request_t>>> g_wait_map;


bool have_cuda_recompute()
{
	return get_cuda_recompute_devices().size();
}

std::vector<cuda_device_t> get_cuda_recompute_devices()
{
	if(!have_init) {
		cuda_recompute_init();
	}
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_device_list;
}

void cuda_recompute_init(const int max_devices, const std::vector<int>& device_list)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if(have_init) {
		return;
	}
	have_init = true;
	// TODO
}

uint64_t cuda_recompute(const int ksize, const int xbits, const hash_t& plot_id, const std::vector<uint32_t>& x_values)
{
	const std::tuple<int, int> type_key(ksize, xbits);

	auto req = std::make_shared<request_t>();
	req->id = next_request_id++;
	req->ksize = ksize;
	req->xbits = xbits;
	req->plot_id = plot_id;
	req->x_values = x_values;

	std::lock_guard<std::mutex> lock(g_mutex);

	if(std::find(g_order_queue.begin(), g_order_queue.end(), type_key) == g_order_queue.end()) {
		g_order_queue.push_back(type_key);
	}
	g_wait_map[type_key].push(req);

	// TODO
}

std::shared_ptr<const cuda_result_t> cuda_recompute_poll(const std::set<uint64_t>& jobs)
{
	// TODO
}













} // pos
} // mmx
