cmake_minimum_required(VERSION 3.13)
project(mmx-node C CXX ASM)

include(FetchContent)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
	include(GenerateExportHeader)
	include_directories(${CMAKE_CURRENT_BINARY_DIR})
	
	add_compile_definitions(TARGET_FILENAME=$(TargetName)$(TargetExt))
	add_compile_options(/openmp /EHsc /MP)

	set(MSVC_DEBUG_OPTIONS /Od /FS)
	set(MSVC_RELEASE_OPTIONS /W1 /O2)

	add_compile_options(
		"$<$<CONFIG:Debug>:${MSVC_DEBUG_OPTIONS}>"
		"$<$<CONFIG:RelWithDebInfo>:${MSVC_RELEASE_OPTIONS}>"
		"$<$<CONFIG:Release>:${MSVC_RELEASE_OPTIONS}>"
		"$<$<CONFIG:MinSizeRel>:${MSVC_RELEASE_OPTIONS}>"
	)

	set(MSVC_DEBUG_LINK_OPTIONS /NODEFAULTLIB:LIBCMT)
	set(MSVC_RELEASE_LINK_OPTIONS /NODEFAULTLIB:LIBCMT)

	add_link_options(
		"$<$<CONFIG:Debug>:${MSVC_DEBUG_LINK_OPTIONS}>"
		"$<$<CONFIG:RelWithDebInfo>:${MSVC_RELEASE_LINK_OPTIONS}>"
		"$<$<CONFIG:Release>:${MSVC_RELEASE_LINK_OPTIONS}>"
		"$<$<CONFIG:MinSizeRel>:${MSVC_RELEASE_LINK_OPTIONS}>"
	)
	
	find_library(secp256k1_LIBRARY secp256k1 HINTS ${secp256k1_LIBRARIES})
else()
	message(STATUS "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

	find_package(OpenMP REQUIRED)
	find_library(JEMALLOC_LIB NAMES jemalloc)
	
	add_compile_options(-Wall -Wno-unused-function -Wno-parentheses -Wno-unused-local-typedefs -Wno-unknown-pragmas)
	
	if(WITH_COVERAGE)
		message(STATUS "Enabling -O0 -coverage")
		add_compile_options(-O0 --coverage)
		add_link_options(-lgcov --coverage)
	endif()
	
	set_source_files_properties(src/vm/Compiler.cpp PROPERTIES COMPILE_FLAGS -Wno-unused-but-set-variable)

	find_library(secp256k1_LIBRARY NAMES secp256k1 PATHS "${CMAKE_CURRENT_SOURCE_DIR}/lib/secp256k1/.libs" NO_DEFAULT_PATH)
	
	if(ENABLE_MEM_SANITIZE)
		message(STATUS "Compiling with -fsanitize=address")
		add_compile_options(-fsanitize=address)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
		set(DISABLE_OPENCL 1)
	elseif(ENABLE_THREAD_SANITIZE)
		message(STATUS "Compiling with -fsanitize=thread")
		add_compile_options(-fsanitize=thread)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
		set(DISABLE_OPENCL 1)
	endif()
endif()

include(cmake/build_version_json.cmake)

add_library(secp256k1 INTERFACE IMPORTED)
set_target_properties(secp256k1 PROPERTIES INTERFACE_LINK_LIBRARIES ${secp256k1_LIBRARY})

get_filename_component(secp256k1_IMPORT_PREFIX "${secp256k1_LIBRARY}" PATH)
get_filename_component(secp256k1_IMPORT_PREFIX "${secp256k1_IMPORT_PREFIX}" PATH)

set_target_properties(secp256k1 PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${secp256k1_IMPORT_PREFIX}/include"
)

file(GLOB secp256k1_LIBRARY_FILES ${secp256k1_LIBRARY}*)

set(LIBBECH32_BUILD_TESTS OFF CACHE BOOL "LIBBECH32_BUILD_TESTS")
set(LIBBECH32_BUILD_EXAMPLES OFF CACHE BOOL "LIBBECH32_BUILD_EXAMPLES")
set(DIRENT_BUILD_TESTS OFF CACHE STRING "DIRENT_BUILD_TESTS")
set(VNX_BUILD_TOOLS ON CACHE STRING "VNX_BUILD_TOOLS")
set(MMX_BUILD_TOOLS ON CACHE STRING "MMX_BUILD_TOOLS")
set(MMX_BUILD_TESTS ON CACHE STRING "MMX_BUILD_TESTS")
set(NODE_INSTALL_PATH "." CACHE STRING "Where to install scripts")

add_subdirectory(vnx-base)
add_subdirectory(vnx-addons)
add_subdirectory(lib/libbech32)

find_package(Threads REQUIRED)
find_library(MINIUPNPC_LIB NAMES miniupnpc)

if(NOT DISABLE_OPENCL)
	find_package(OpenCL)
endif()
if(NOT DISABLE_CUDA)
	include(cmake/use_cuda.cmake)
endif()
if(NOT DISABLE_QT)
	find_package(Qt5 QUIET COMPONENTS Widgets WebEngine WebEngineWidgets)
endif()

file(GLOB GENERATED_SOURCE_FILES
	generated/src/*.cpp
	generated/vm/src/*.cpp
	generated/contract/src/*.cpp
	generated/operation/src/*.cpp
	generated/solution/src/*.cpp
)

add_library(mmx_iface SHARED
	${GENERATED_SOURCE_FILES}
	src/Block.cpp
	src/BlockHeader.cpp
	src/Transaction.cpp
	src/Operation.cpp
	src/Contract.cpp
	src/Solution.cpp
	src/ProofOfTime.cpp
	src/ProofOfSpace.cpp
	src/ProofOfSpaceOG.cpp
	src/ProofOfSpaceNFT.cpp
	src/ProofResponse.cpp
	src/secp256k1.cpp
	src/uint128.cpp
	src/fixed128.cpp
	src/hash_t.cpp
	src/hash_512_t.cpp
	src/addr_t.cpp
	src/pubkey_t.cpp
	src/signature_t.cpp
	src/exec_result_t.cpp
	src/exec_error_t.cpp
	src/vm/var_t.cpp
	src/vm/varptr_t.cpp
	src/contract/Data.cpp
	src/contract/WebData.cpp
	src/contract/PubKey.cpp
	src/contract/MultiSig.cpp
	src/contract/TokenBase.cpp
	src/contract/Binary.cpp
	src/contract/Executable.cpp
	src/contract/method_t.cpp
	src/solution/PubKey.cpp
	src/solution/MultiSig.cpp
	src/operation/Deposit.cpp
	src/operation/Execute.cpp
	src/sha256_avx2.cpp
	src/sha256_64_x8.cpp
	src/sha256_ni.cpp
	src/sha256_ni_rec.cpp
	src/sha256_arm.cpp
	src/sha256_arm_rec.cpp
	src/sha512.cpp
	src/hmac_sha512.cpp
	src/wordlist_en.cpp
	src/mnemonic.cpp
	src/offer_data_t.cpp
	src/account_info_t.cpp
	src/VDF_Point.cpp
	src/Partial.cpp
	src/ChainParams.cpp
	src/utils.cpp
	src/ValidatorVote.cpp
)

add_library(mmx_vm STATIC
	src/vm/Engine.cpp
	src/vm/Compiler.cpp
	src/vm/StorageProxy.cpp
	src/vm/StorageRAM.cpp
	src/vm/StorageCache.cpp
	src/vm/StorageDB.cpp
	src/vm/instr_t.cpp
	src/vm_interface.cpp
)

add_library(mmx_db STATIC
	src/DataBase.cpp
	src/table.cpp
)

add_library(mmx_pos STATIC
	src/pos/mem_hash.cpp
	src/pos/verify.cpp
	src/pos/encoding.cpp
	src/pos/Prover.cpp
)

add_library(mmx_pos_verify STATIC
	src/pos/verify_full.cpp
)

add_library(mmx_modules STATIC
	src/TimeLord.cpp
	src/Node.cpp
	src/Node_api.cpp
	src/Node_update.cpp
	src/Node_verify.cpp
	src/Node_validate.cpp
	src/Node_control.cpp
	src/Node_tests.cpp
	src/Wallet.cpp
	src/Farmer.cpp
	src/Harvester.cpp
	src/Router.cpp
	src/WebAPI.cpp
	src/OCL_VDF.cpp
	src/upnp_mapper.cpp
	src/http_request.cpp
	src/ProofServer.cpp
)

add_library(mmx_qtgui STATIC
	src/Qt_GUI.cpp
)

add_library(uint256_t STATIC
	uint256_t/uint128_t/uint128_t.cpp
	uint256_t/uint256_t.cpp
)

target_include_directories(mmx_vm
	PUBLIC lib/lexy/include
)

target_include_directories(mmx_pos
	PUBLIC include
)

target_include_directories(mmx_iface
	PUBLIC
		include
		generated/include
		generated/vm/include
		generated/contract/include
		generated/operation/include
		generated/solution/include
)

target_include_directories(mmx_qtgui
	PUBLIC include
)

target_include_directories(uint256_t
	PUBLIC uint256_t uint256_t/uint128_t
)

target_compile_definitions(uint256_t PUBLIC _UINT256_T_STATIC _UINT128_T_STATIC)

target_link_libraries(mmx_iface vnx_base vnx_addons uint256_t bech32 secp256k1)
target_link_libraries(mmx_db mmx_iface)
target_link_libraries(mmx_pos mmx_pos_verify mmx_iface)
target_link_libraries(mmx_pos_verify mmx_iface)
target_link_libraries(mmx_vm mmx_db mmx_iface)
target_link_libraries(mmx_modules mmx_pos mmx_vm mmx_db mmx_iface)
target_link_libraries(mmx_qtgui vnx_base)

if(MSVC)
	set_target_properties(vnx_base_static PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)

	GENERATE_EXPORT_HEADER(mmx_iface)
	
	set_target_properties(mmx_iface PROPERTIES ENABLE_EXPORTS 1)
	set_target_properties(mmx_iface PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)

	# Workaround for:
	#	LNK1189: library limit of 65535 objects exceeded
	set(MSVC_MMX_IFACE_DEBUG_OPTIONS /Ob1)
	target_compile_options(mmx_iface PRIVATE
		"$<$<CONFIG:Debug>:${MSVC_MMX_IFACE_DEBUG_OPTIONS}>"
	)

else()
	target_link_libraries(mmx_db OpenMP::OpenMP_CXX)
	target_link_libraries(mmx_modules OpenMP::OpenMP_CXX)
	
	if(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86_64")
		message(STATUS "Enabling -mavx2 -msha")
		set_source_files_properties(src/sha256_ni.cpp PROPERTIES COMPILE_FLAGS "-msse4.2 -msha")
		set_source_files_properties(src/sha256_ni_rec.cpp PROPERTIES COMPILE_FLAGS "-msse4.2 -msha")
		set_source_files_properties(src/sha256_avx2.cpp PROPERTIES COMPILE_FLAGS "-mavx2")
	endif()

	if(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "aarch64")
		message(STATUS "Enabling -march=armv8-a+crypto")
		set_source_files_properties(src/sha256_arm.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a+crypto")
		set_source_files_properties(src/sha256_arm_rec.cpp PROPERTIES COMPILE_FLAGS "-march=armv8-a+crypto")
	endif()
endif()

if(OpenCL_FOUND)
	message(STATUS "Found OpenCL")
	add_subdirectory(basic-opencl)
	target_compile_definitions(automy_basic_opencl
		PUBLIC CL_TARGET_OPENCL_VERSION=120
	)
	target_link_libraries(mmx_modules automy_basic_opencl)
	target_compile_definitions(mmx_modules PUBLIC WITH_OPENCL)
endif()

if(CUDA_FOUND)
	message(STATUS "Found CUDA")
	add_compile_definitions(WITH_CUDA)
	cuda_add_library(mmx_pos_cuda STATIC src/pos/cuda_recompute.cu)
	target_include_directories(mmx_pos_cuda PUBLIC include)
	target_link_libraries(mmx_pos mmx_pos_cuda)
	target_link_libraries(mmx_pos_cuda mmx_pos_verify mmx_iface)
	
	if(MSVC)
	    # Copy CUDA DLLs to the output folder
	    add_custom_command(TARGET mmx_modules
	        POST_BUILD
	        COMMAND ${CMAKE_COMMAND} -E copy_if_different
	        "${CUDA_TOOLKIT_ROOT_DIR}/bin/cudart64_12.dll"
	        $<TARGET_FILE_DIR:mmx_modules>
	    )
	endif()
endif()

if(JEMALLOC_LIB)
	message(STATUS "Found jemalloc")
	target_link_libraries(mmx_modules ${JEMALLOC_LIB})
	target_compile_definitions(mmx_modules PUBLIC WITH_JEMALLOC)
endif()

if(MINIUPNPC_LIB)
	message(STATUS "Found miniupnpc")
	target_link_libraries(mmx_modules ${MINIUPNPC_LIB})
	target_compile_definitions(mmx_modules PRIVATE WITH_MINIUPNPC)
endif()

if(Qt5_FOUND)
	message(STATUS "Found Qt5")
	add_compile_definitions(WITH_QT)
	target_link_libraries(mmx_qtgui Qt5::Widgets Qt5::WebEngine Qt5::WebEngineWidgets)
endif()

add_executable(mmx src/mmx.cpp)
add_executable(mmx_node src/mmx_node.cpp)
add_executable(mmx_farmer src/mmx_farmer.cpp)
add_executable(mmx_wallet src/mmx_wallet.cpp)
add_executable(mmx_timelord src/mmx_timelord.cpp)
add_executable(mmx_harvester src/mmx_harvester.cpp)

target_link_libraries(mmx mmx_iface mmx_vm)
target_link_libraries(mmx_node mmx_modules mmx_qtgui)
target_link_libraries(mmx_farmer mmx_modules)
target_link_libraries(mmx_wallet mmx_modules mmx_qtgui)
target_link_libraries(mmx_timelord mmx_modules)
target_link_libraries(mmx_harvester mmx_modules)

install(TARGETS mmx_iface DESTINATION lib)

install(TARGETS mmx DESTINATION bin)
install(TARGETS mmx_node DESTINATION bin)
install(TARGETS mmx_farmer DESTINATION bin)
install(TARGETS mmx_wallet DESTINATION bin)
install(TARGETS mmx_timelord DESTINATION bin)
install(TARGETS mmx_harvester DESTINATION bin)

install(DIRECTORY
	config data kernel scripts
	DESTINATION ${NODE_INSTALL_PATH}
	PATTERN "config/local" EXCLUDE
)
install(DIRECTORY
	ui/dist/gui
	DESTINATION ${NODE_INSTALL_PATH}/ui/dist
	PATTERN "node_modules" EXCLUDE
)
install(FILES
	activate.sh run_node.sh run_farmer.sh run_wallet.sh run_wallet_gui.sh run_timelord.sh run_harvester.sh
	start_node.sh start_harvester.sh start_wallet.sh stop_node.sh stop_harvester.sh stop_wallet.sh
	PERMISSIONS OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
	DESTINATION ${NODE_INSTALL_PATH}
)
install(FILES
	cmake/debian/mmx_node.desktop cmake/debian/mmx_wallet.desktop
	PERMISSIONS OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
	DESTINATION share/applications
)

if(MMX_BUILD_TOOLS)
	add_subdirectory(tools)
endif()
if(MMX_BUILD_TESTS)
	add_subdirectory(test)
endif()

if(MSVC)
	include(cmake/win_pack.cmake)
endif()
