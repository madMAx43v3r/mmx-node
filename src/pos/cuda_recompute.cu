/*
 * mem_hash.cu
 *
 *  Created on: Feb 2, 2025
 *      Author: mad
 */

#include <mmx/pos/cuda_recompute.h>
#include <mmx/pos/verify.h>
#include <vnx/ThreadPool.h>
#include <vnx/vnx.h>

#include <cuda_sha512.h>
#include <cuda_runtime.h>

#include <mutex>
#include <tuple>
#include <queue>
#include <atomic>
#include <thread>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>
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
void cuda_gen_mem_array(uint4* mem_out, uint4* key_out, uint32_t* X_out, const uint32_t* X_in, const uint32_t* ID_in,
						const int xbits, const uint32_t y_0)
{
	const uint32_t x = threadIdx.x;
	const uint32_t y = blockIdx.x;
	const uint32_t z = blockIdx.y;
	const uint32_t out = ((z * gridDim.x + y) * blockDim.x) + x;
	const uint32_t num_entries = gridDim.y * gridDim.x * blockDim.x;

	const uint32_t X_i = (X_in[z * blockDim.x + x] << xbits) | (y_0 + y);
	X_out[out] = X_i;

	__align__(8) uint32_t msg[32] = {};

	msg[0] = X_i;

	for(int i = 0; i < 8; ++i) {
		msg[1 + i] = ID_in[z * 8 + i];
	}
	__align__(8) uint32_t key[16] = {};

	cuda_sha512((uint64_t*)msg, 4 + 32, (uint64_t*)key);

	for(int i = 0; i < 4; ++i) {
		key_out[out * 4 + i] = make_uint4(key[i * 4 + 0], key[i * 4 + 1], key[i * 4 + 2], key[i * 4 + 3]);
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

	for(uint32_t i = 0; i < 32; ++i)
	{
		for(int j = 0; j < 4; ++j) {
#pragma unroll
			for(int k = 0; k < 16; ++k) {
				MMXPOS_HASHROUND(state[k], b, c, state[16 + k]);
			}
		}

#pragma unroll
		for(int k = 0; k < 8; ++k) {
			mem_out[(uint64_t(i) * num_entries + out) * 8 + k] =
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
	bool failed = false;
	uint64_t buffer_size = 0;

	uint32_t* X_buf = nullptr;			//    4 bytes
	uint32_t* ID_buf = nullptr;			//   32 bytes

	uint32_t* X_dev = nullptr;			//    4 bytes
	uint32_t* X_out = nullptr;			//    4 bytes
	uint32_t* ID_dev = nullptr;			//   32 bytes
	uint32_t* key_dev = nullptr;		//   64 bytes
	uint32_t* mem_dev = nullptr;		// 4096 bytes
	uint32_t* hash_dev = nullptr;		//  128 bytes
	uint32_t* M_dev = nullptr;			//   64 bytes
	uint32_t* Y_dev = nullptr;			//    4 bytes

	uint32_t* Y_buf = nullptr;			//    4 bytes
	uint32_t* M_buf = nullptr;			//   64 bytes

	std::thread thread;

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

class hardware_error_t : public std::runtime_error {
public:
	hardware_error_t(const std::string& msg) : runtime_error(msg) {}
};

std::mutex g_mutex;
std::condition_variable g_result_signal;
std::condition_variable g_request_signal;

std::atomic<bool> do_run {true};
std::atomic<bool> have_init {false};
std::atomic<int> have_cuda {0};
std::vector<cuda_device_t> g_device_list;
std::vector<std::shared_ptr<device_t>> g_devices;

std::atomic<uint64_t> next_request_id {1};
std::deque<std::tuple<int, int>> g_order_queue;
std::unordered_map<uint64_t, std::shared_ptr<const cuda_result_t>> g_result_map;
std::map<std::tuple<int, int>, std::queue<std::shared_ptr<request_t>>> g_wait_map;

std::shared_ptr<vnx::ThreadPool> g_cpu_threads;


inline void cuda_check(const cudaError_t& code) {
	if(code != cudaSuccess) {
		throw hardware_error_t(std::string(cudaGetErrorString(code)));
	}
}

bool have_cuda_recompute() {
	return do_run && have_cuda > 0;
}

std::vector<cuda_device_t> get_cuda_devices()
{
	int num_devices = 0;
	cudaGetDeviceCount(&num_devices);

	std::vector<cuda_device_t> list;
	for(int i = 0; i < num_devices; ++i) {
		cudaDeviceProp info;
		cudaGetDeviceProperties(&info, i);
		if(info.major >= 5) {
			cuda_device_t dev;
			dev.index = i;
			dev.name = info.name;
			dev.max_resident = info.multiProcessorCount * info.maxThreadsPerMultiProcessor;
			list.push_back(dev);
		}
	}
	return list;
}

std::vector<cuda_device_t> get_cuda_devices_used()
{
	if(!have_init) {
		cuda_recompute_init();
	}
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_device_list;
}

static void cuda_recompute_loop(std::shared_ptr<device_t> dev);

void cuda_recompute_init(bool enable, std::vector<int> device_list)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if(have_init) {
		return;
	}
	have_init = true;

	if(device_list.empty()) {
		vnx::read_config("cuda.devices", device_list);
	}
	vnx::read_config("cuda.enable", enable);

	if(!enable) {
		vnx::log_info() << "CUDA compute is disabled";
		return;
	}
	auto list = get_cuda_devices();

	if(device_list.empty()) {
		g_device_list = list;
	} else {
		for(size_t i : device_list) {
			if(i < list.size()) {
				g_device_list.push_back(list[i]);
			}
		}
	}
	const auto num_threads = std::max(std::thread::hardware_concurrency(), 4u);
	g_cpu_threads = std::make_shared<vnx::ThreadPool>(num_threads, num_threads);

	vnx::log_info() << "Using " << num_threads << " CPU threads for CUDA recompute";

	for(auto& info : g_device_list)
	{
		info.buffer_size = 256;
		while(info.buffer_size < info.max_resident) {
			info.buffer_size <<= 1;
		}
		info.buffer_size /= 2;

		const int num_threads = (info.max_resident * 3 + info.buffer_size - 1) / info.buffer_size;

		for(int i = 0; i < num_threads; ++i) {
			auto dev = std::make_shared<device_t>();
			dev->index = info.index;
			dev->buffer_size = info.buffer_size;
			dev->thread = std::thread(&cuda_recompute_loop, dev);
			g_devices.push_back(dev);
		}
		vnx::log_info() << "Using CUDA device '" << info.name
				<< "' [" << info.index << "] with threads " << info.max_resident << ", buffer " << info.buffer_size << "x" << num_threads;
	}
	have_cuda = g_devices.size();

	if(!have_cuda) {
		if(list.empty()) {
			vnx::log_info() << "No CUDA devices found!";
		} else {
			vnx::log_info() << "No CUDA devices enabled!";
		}
	}
}

void cuda_recompute_shutdown()
{
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		do_run = false;
	}
	g_request_signal.notify_all();

	for(auto dev : g_devices) {
		dev->thread.join();
	}
	g_devices.clear();
	g_device_list.clear();
}

uint64_t cuda_recompute(const int ksize, const int xbits, const hash_t& plot_id, const std::vector<uint32_t>& x_values)
{
	if(ksize < 8 || ksize > 32) {
		throw std::logic_error("invalid ksize");
	}
	if(xbits < 0 || xbits > 20 || xbits + 8 >= ksize) {
		throw std::logic_error("invalid xbits");
	}
	if(x_values.size() != 256) {
		throw std::logic_error("invalid x_values");
	}
	if(!have_init) {
		cuda_recompute_init();
	}
	const std::tuple<int, int> type(ksize, xbits);

	auto req = std::make_shared<request_t>();
	req->id = next_request_id++;
	req->ksize = ksize;
	req->xbits = xbits;
	req->plot_id = plot_id;
	req->x_values = x_values;
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		if(!do_run || have_cuda <= 0) {
			throw std::logic_error("no CUDA devices available");
		}
		if(std::find(g_order_queue.begin(), g_order_queue.end(), type) == g_order_queue.end()) {
			g_order_queue.push_back(type);
		}
		g_wait_map[type].push(req);
	}
	g_request_signal.notify_all();
	return req->id;
}

std::shared_ptr<const cuda_result_t> cuda_recompute_poll(const std::set<uint64_t>& jobs)
{
	while(true) {
		std::unique_lock<std::mutex> lock(g_mutex);
		if(!g_result_map.empty()) {
			for(const auto id : jobs) {
				const auto iter = g_result_map.find(id);
				if(iter != g_result_map.end()) {
					const auto res = iter->second;
					g_result_map.erase(iter);
					return res;
				}
			}
		}
		if(!do_run) {
			throw std::logic_error("shutdown");
		}
		g_result_signal.wait(lock);
	}
}

static void cuda_finish_cpu(std::shared_ptr<request_t> req)
{
	auto res = std::make_shared<cuda_result_t>();
	res->id = req->id;
	try {
		std::vector<uint32_t> X_out;
		const auto entries = compute_full(req->X_tmp, req->Y_tmp, req->M_tmp, &X_out, req->plot_id, req->ksize);
		res->X = std::move(X_out);
		res->entries = entries;
	}
	catch(const std::exception& ex) {
		res->failed = true;
		res->error = ex.what();
	}
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		g_result_map[res->id] = res;
	}
	g_result_signal.notify_all();
}

static void cuda_recompute_loop(std::shared_ptr<device_t> dev)
{
	cudaStream_t stream;
	try {
		cuda_check(cudaSetDevice(dev->index));
		cuda_check(cudaDeviceSynchronize());
		cuda_check(cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync));
		cuda_check(cudaStreamCreate(&stream));

		cuda_check(cudaMallocHost(&dev->X_buf,  dev->buffer_size * 4));
		cuda_check(cudaMallocHost(&dev->Y_buf,  dev->buffer_size * 4));
		cuda_check(cudaMallocHost(&dev->ID_buf, dev->buffer_size / 256 * 32));
		cuda_check(cudaMallocHost(&dev->M_buf,  dev->buffer_size * 64));

		cuda_check(cudaMalloc(&dev->X_dev, 		dev->buffer_size * 4));
		cuda_check(cudaMalloc(&dev->X_out, 		dev->buffer_size * 4));
		cuda_check(cudaMalloc(&dev->ID_dev, 	dev->buffer_size / 256 * 32));
		cuda_check(cudaMalloc(&dev->key_dev, 	dev->buffer_size * 64));
		cuda_check(cudaMalloc(&dev->mem_dev, 	dev->buffer_size * 4096));
		cuda_check(cudaMalloc(&dev->hash_dev, 	dev->buffer_size * 128));
		cuda_check(cudaMalloc(&dev->M_dev, 		dev->buffer_size * 64));
		cuda_check(cudaMalloc(&dev->Y_dev, 		dev->buffer_size * 4));
	}
	catch(const std::exception& ex) {
		dev->failed = true;
		vnx::log_error() << "CUDA: failed to allocate memory for device " << dev->index << ": " << ex.what();
		goto failed;
	}

	while(do_run) {
		std::unique_lock<std::mutex> lock(g_mutex);
		while(do_run && g_order_queue.empty()) {
			g_request_signal.wait(lock);
		}
		if(!do_run) {
			break;
		}
		const auto type = g_order_queue.front();
		g_order_queue.pop_front();

		const auto ksize = std::get<0>(type);
		const auto xbits = std::get<1>(type);
		const auto req_size = uint64_t(256) << xbits;

		uint64_t alloc_sum = 0;
		std::vector<std::shared_ptr<request_t>> req_list;
		req_list.reserve(dev->buffer_size / req_size);
		{
			auto& req_queue = g_wait_map[type];
			while(!req_queue.empty()) {
				if(!alloc_sum || alloc_sum + req_size <= dev->buffer_size) {
					req_list.push_back(req_queue.front());
					alloc_sum += req_size;
					req_queue.pop();
				} else {
					break;
				}
			}
			if(!req_queue.empty()) {
				g_order_queue.push_back(type);
			}
		}
		lock.unlock();

		uint32_t num_iter = 1;
		while(req_size / num_iter > dev->buffer_size) {
			num_iter <<= 1;
		}
		const uint64_t N = (1u << xbits) / num_iter;
		const uint64_t M = req_list.size();
		const uint64_t grid_size = uint64_t(256) * N * M;
		const uint32_t KMASK = (uint64_t(1) << ksize) - 1;

		std::unordered_set<uint32_t> x_set;

		for(uint64_t i = 0; i < M; ++i) {
			const auto& req = req_list[i];
			req->X_tmp.resize(req_size);
			req->Y_tmp.resize(req_size);
			req->M_tmp.resize(req_size);
			for(auto& x : req->x_values) {
				while(!x_set.insert(x).second) {
					x++;	// avoid duplicate inputs
				}
			}
			::memcpy(dev->X_buf + i * 256, req->x_values.data(), 256 * 4);
			::memcpy(dev->ID_buf + i * 8, req->plot_id.data(), 32);
			x_set.clear();
		}

		cudaMemcpyAsync(dev->X_dev, dev->X_buf,   M * 256 * 4, cudaMemcpyHostToDevice, stream);
		cudaMemcpyAsync(dev->ID_dev, dev->ID_buf, M * 32,      cudaMemcpyHostToDevice, stream);

		for(uint32_t iter = 0; iter < num_iter; ++iter)
		{
			const uint64_t y_0 = iter * N;
			{
				dim3 block(256, 1);
				dim3 grid(N, M);
				cuda_gen_mem_array<<<grid, block, 0, stream>>>(
						(uint4*)dev->mem_dev,
						(uint4*)dev->key_dev,
						dev->X_out,
						dev->X_dev,
						dev->ID_dev,
						xbits, y_0);
			}
			{
				dim3 block(32, 4);
				dim3 grid(1, grid_size / block.y / 64, 64);
				cuda_calc_mem_hash<<<grid, block, 0, stream>>>(
						dev->mem_dev,
						dev->hash_dev,
						MEM_HASH_ITER);
			}
			{
				dim3 block(256, 1);
				dim3 grid(N * M, 1);
				cuda_final_mem_hash<<<grid, block, 0, stream>>>(
						(uint4*)dev->M_dev,
						        dev->Y_dev,
						(uint4*)dev->hash_dev,
						(uint4*)dev->key_dev,
						KMASK);
			}
			cudaMemcpyAsync(dev->X_buf, dev->X_out, grid_size * 4,  cudaMemcpyDeviceToHost, stream);
			cudaMemcpyAsync(dev->Y_buf, dev->Y_dev, grid_size * 4,  cudaMemcpyDeviceToHost, stream);
			cudaMemcpyAsync(dev->M_buf, dev->M_dev, grid_size * 64, cudaMemcpyDeviceToHost, stream);

			const auto err = cudaStreamSynchronize(stream);
			if(err != cudaSuccess) {
				std::lock_guard<std::mutex> lock(g_mutex);
				for(const auto& req : req_list) {
					auto res = std::make_shared<cuda_result_t>();
					res->id = req->id;
					res->failed = true;
					res->error = "CUDA error";
					g_result_map[res->id] = res;
				}
				g_result_signal.notify_all();
				dev->failed = true;
				vnx::log_error() << "CUDA: error " << err << ": " << cudaGetErrorString(err);
				goto failed;
			}

			for(uint64_t i = 0; i < M; ++i) {
				const auto& req = req_list[i];
				const uint64_t count = 256 * N;
				::memcpy(req->X_tmp.data() + iter * count, dev->X_buf + i * count, count * 4);
				::memcpy(req->Y_tmp.data() + iter * count, dev->Y_buf + i * count, count * 4);
				for(uint64_t k = 0; k < count; ++k) {
					::memcpy(req->M_tmp.data() + iter * count + k, dev->M_buf + (i * count + k) * 16, N_META * 4);
				}
			}
		}

		for(const auto& req : req_list) {
			g_cpu_threads->add_task(std::bind(&cuda_finish_cpu, req));
		}
	}

failed:
	have_cuda--;
}





} // pos
} // mmx
