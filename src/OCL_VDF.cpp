/*
 * OCL_VDF.cpp
 *
 *  Created on: Dec 23, 2021
 *      Author: mad
 */

#include <mmx/OCL_VDF.h>

#include <vnx/vnx.h>


namespace mmx {

std::mutex OCL_VDF::g_mutex;
std::shared_ptr<OCL_VDF::Program> OCL_VDF::g_program;


OCL_VDF::OCL_VDF(cl_uint device)
{
	std::lock_guard<std::mutex> lock(g_mutex);

	if(!g_program) {
		std::string kernel_path = "kernel/";
		vnx::read_config("opencl.kernel_path", kernel_path);
		automy::basic_opencl::add_include_path(kernel_path);

		auto program = Program::create();
		program->add_source("sha256.cl");
		program->add_source("rsha256.cl");
		program->create_from_source();
		if(!program->build()) {
			std::string text;
			for(const auto& line : program->build_log) {
				text += line + "\n";
			}
			vnx::log_error() << "OCL_VDF: build failed with:" << std::endl << text;
			throw std::runtime_error("build failed");
		}
		g_program = program;
	}
	kernel = g_program->create_kernel("rsha256_kernel");

	if(!kernel) {
		throw std::runtime_error("rsha256 missing");
	}
	queue = automy::basic_opencl::create_command_queue(device);
}

void OCL_VDF::compute(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain, const hash_t& begin)
{
	const size_t local = 64;
	const size_t width = proof->segments.size() + (local - (proof->segments.size() % local)) % local;
	if(!width) {
		throw std::logic_error("no segments");
	}
	hash.resize(width * 32);
	{
		auto start_iters = proof->start;
		for(size_t i = 0; i < proof->segments.size(); ++i)
		{
			auto point = i > 0 ? proof->segments[i-1].output[chain] : begin;
			{
				auto iter = proof->infuse[chain].find(start_iters);
				if(iter != proof->infuse[chain].end()) {
					point = hash_t(point + iter->second);
				}
			}
			::memcpy(hash.data() + i * 32, point.data(), point.size());
			start_iters += proof->segments[i].num_iters;
		}
	}

	num_iters.resize(width);
	uint32_t max_num_iters = 0;
	for(size_t i = 0; i < proof->segments.size(); ++i) {
		const auto& seg = proof->segments[i];
		num_iters[i] = seg.num_iters;
		max_num_iters = std::max(max_num_iters, seg.num_iters);
	}

	hash_buf.upload(queue, hash, false);
	num_iters_buf.upload(queue, num_iters, false);

	kernel->set("hash", hash_buf);
	kernel->set("num_iters", num_iters_buf);

	for(uint32_t k = 0; k < max_num_iters; k += 4096) {
		kernel->enqueue(queue, width, local);
	}
	queue->flush();
}

void OCL_VDF::verify(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain)
{
	hash_buf.download(queue, hash.data(), true);

	for(size_t i = 0; i < proof->segments.size(); ++i)
	{
		if(::memcmp(hash.data() + i * 32, proof->segments[i].output[chain].data(), 32))
		{
			throw std::logic_error("invalid output at segment " + std::to_string(i));
		}
	}
}



} // mmx
