
include(CheckLanguage)

check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
	enable_language(CUDA)
	set(MMX_CUDA_FOUND TRUE)

	# Keep the toolkit location available for packaging the CUDA runtime on Windows.
	get_filename_component(MMX_CUDA_TOOLKIT_BIN_DIR "${CMAKE_CUDA_COMPILER}" DIRECTORY)
else()
	set(MMX_CUDA_FOUND FALSE)
endif()
