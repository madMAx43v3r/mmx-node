/*
 * OCL_VDF.cpp
 *
 *  Created on: Dec 23, 2021
 *      Author: mad
 */

#include <mmx/OCL_VDF.h>

#include <vnx/vnx.h>


namespace mmx {

#ifdef WITH_OPENCL

std::mutex OCL_VDF::g_mutex;
std::shared_ptr<OCL_VDF::Program> OCL_VDF::g_program;


OCL_VDF::OCL_VDF(cl_context context, cl_device_id device)
	:	context(context)
{
	std::lock_guard<std::mutex> lock(g_mutex);

	if(!g_program) {
		std::string kernel_path = "kernel/";
		vnx::read_config("opencl.kernel_path", kernel_path);

		auto program = Program::create(context);
		program->add_include_path(kernel_path);
		program->add_source("sha256.cl");
		program->add_source("rsha256.cl");
		program->create_from_source();
		if(!program->build({device})) {
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
	queue = automy::basic_opencl::create_command_queue(context, device);
}

void OCL_VDF::compute(std::shared_ptr<const ProofOfTime> proof)
{
	const size_t local = 64;
	const size_t width = proof->segments.size() + (local - (proof->segments.size() % local)) % local;
	if(!width) {
		throw std::logic_error("no segments");
	}
	hash.resize(width * 32);
	{
		auto input = proof->input;
		if(proof->prev) {
			input = hash_t(input + (*proof->prev));
		}
		if(proof->reward_addr) {
			input = hash_t(input + (*proof->reward_addr));
		}
		for(size_t i = 0; i < proof->segments.size(); ++i)
		{
			::memcpy(hash.data() + i * 32, input.data(), input.size());
			input = proof->segments[i];
		}
	}

	num_iters.resize(width);
	for(size_t i = 0; i < proof->segments.size(); ++i) {
		num_iters[i] = proof->segment_size;
	}

	hash_buf.alloc_min(context, width * 32);
	num_iters_buf.alloc_min(context, width);

	hash_buf.upload(queue, hash, false);
	num_iters_buf.upload(queue, num_iters, false);

	const uint32_t max_iters = 5000;

	kernel->set("hash", hash_buf);
	kernel->set("num_iters", num_iters_buf);
	kernel->set("max_iters", max_iters);

	for(uint32_t k = 0; k < proof->segment_size; k += max_iters) {
		kernel->enqueue(queue, width, local);
	}
	queue->flush();
}

void OCL_VDF::verify(std::shared_ptr<const ProofOfTime> proof)
{
	hash_buf.download(queue, hash.data(), true);

	for(size_t i = 0; i < proof->segments.size(); ++i)
	{
		if(::memcmp(hash.data() + i * 32, proof->segments[i].data(), 32)) {
			throw std::logic_error("invalid output at segment " + std::to_string(i));
		}
	}
}

void OCL_VDF::release()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	g_program = nullptr;
}

#else

OCL_VDF::OCL_VDF(cl_context context, cl_device_id device) {
	throw std::logic_error("did not compile with OpenCL support");
}

void OCL_VDF::compute(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain) {}

void OCL_VDF::verify(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain) {}

void OCL_VDF::release() {}

#endif // WITH_OPENCL

} // mmx
