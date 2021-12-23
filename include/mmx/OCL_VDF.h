/*
 * OCL_VDF.h
 *
 *  Created on: Dec 23, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_OCL_VDF_H_
#define INCLUDE_MMX_OCL_VDF_H_

#include <mmx/ProofOfTime.hxx>

#include <automy/basic_opencl/Context.h>
#include <automy/basic_opencl/Program.h>
#include <automy/basic_opencl/Kernel.h>
#include <automy/basic_opencl/Buffer3D.h>


namespace mmx {

class OCL_VDF {
public:
	OCL_VDF(cl_uint device);

	void compute(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain, const hash_t& begin);

	void verify(std::shared_ptr<const ProofOfTime> proof, const uint32_t chain);

private:
	using Kernel = automy::basic_opencl::Kernel;
	using Program = automy::basic_opencl::Program;
	using CommandQueue = automy::basic_opencl::CommandQueue;

	template<typename T>
	using Buffer3D = automy::basic_opencl::Buffer3D<T>;

	size_t width = 0;
	std::vector<cl_uchar> hash;
	std::vector<cl_uint> num_iters;

	Buffer3D<cl_uchar> hash_buf;
	Buffer3D<cl_uint> num_iters_buf;

	std::shared_ptr<Kernel> kernel;
	std::shared_ptr<CommandQueue> queue;

	static std::mutex g_mutex;
	static std::shared_ptr<Program> g_program;

};


} // mmx

#endif /* INCLUDE_MMX_OCL_VDF_H_ */
