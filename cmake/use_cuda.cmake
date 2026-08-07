
if(POLICY CMP0146)
	# Keep using the legacy FindCUDA module until the CUDA build is migrated to first-class CUDA language support.
	cmake_policy(SET CMP0146 OLD)
endif()

find_package(CUDA)

if(NOT MSVC)
	set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} -Xcompiler ,-fPIC")
endif()

set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} -diag-suppress 611,997,68,186 -gencode arch=compute_61,code=compute_61 -gencode arch=compute_75,code=compute_75 -gencode arch=compute_86,code=compute_86")
