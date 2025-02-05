
find_package(CUDA)

if(NOT MSVC)
	set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} -Xcompiler ,-fPIC")
endif()

set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} -diag-suppress 611,997,68,186 -gencode arch=compute_50,code=compute_50 -gencode arch=compute_75,code=compute_75 -gencode arch=compute_86,code=compute_86")
