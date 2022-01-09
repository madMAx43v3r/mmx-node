/*
 * OCL_VDF.h
 *
 *  Created on: Dec 23, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_OCL_VDF_H_
#define INCLUDE_MMX_OCL_VDF_H_

#include <mmx/ProofOfTime.hxx>

#ifdef WITH_OPENCL
#include <automy/basic_opencl/Context.h>
#include <automy/basic_opencl/Program.h>
#include <automy/basic_opencl/Kernel.h>
#include <automy/basic_opencl/Buffer3D.h>
#endif

namespace mmx {

class OCL_VDF {
public:
	OCL_VDF(uint32_t device);

	void compute(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain, const hash_t& begin);

	void verify(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain);

private:
#ifdef WITH_OPENCL
	using Kernel = automy::basic_opencl::Kernel;
	using Program = automy::basic_opencl::Program;
	using CommandQueue = automy::basic_opencl::CommandQueue;

	template<typename T>
	using Buffer3D = automy::basic_opencl::Buffer3D<T>;

	size_t width = 0;
	std::vector<uint8_t> hash;
	std::vector<uint32_t> num_iters;

	Buffer3D<uint8_t> hash_buf;
	Buffer3D<uint32_t> num_iters_buf;

	std::shared_ptr<Kernel> kernel;
	std::shared_ptr<CommandQueue> queue;

	static std::mutex g_mutex;
	static std::shared_ptr<Program> g_program;
#endif

};


} // mmx

#endif /* INCLUDE_MMX_OCL_VDF_H_ */
