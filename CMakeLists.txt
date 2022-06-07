cmake_minimum_required(VERSION 3.11)
project(mmx-node C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-function")
endif()

if(MSVC)
	add_definitions(/openmp ${CUSTOM_ADD_DEFINITIONS})
	
	find_library(secp256k1_LIBRARY secp256k1 HINTS ${secp256k1_LIBRARIES})
	add_library(secp256k1 INTERFACE IMPORTED)
	set_target_properties(secp256k1 PROPERTIES INTERFACE_LINK_LIBRARIES ${secp256k1_LIBRARY})

	include_directories(${CMAKE_CURRENT_BINARY_DIR})
endif()

set(ARITH "easy" CACHE STRING "BLS ARITH")
set(BUILD_BLS_PYTHON_BINDINGS OFF CACHE BOOL "BUILD_BLS_PYTHON_BINDINGS")
set(BUILD_BLS_TESTS OFF CACHE BOOL "BUILD_BLS_TESTS")
set(BUILD_BLS_BENCHMARKS OFF CACHE BOOL "BUILD_BLS_BENCHMARKS")
set(LIBBECH32_BUILD_TESTS OFF CACHE BOOL "LIBBECH32_BUILD_TESTS")
set(LIBBECH32_BUILD_EXAMPLES OFF CACHE BOOL "LIBBECH32_BUILD_EXAMPLES")
set(DIRENT_BUILD_TESTS OFF CACHE STRING "DIRENT_BUILD_TESTS")
set(VNX_BUILD_TOOLS ON CACHE STRING "VNX_BUILD_TOOLS")

add_subdirectory(vnx-base)
add_subdirectory(vnx-addons)
add_subdirectory(vnx-rocksdb)
add_subdirectory(libbech32)
add_subdirectory(bls-signatures)
#add_subdirectory(exchange)
add_subdirectory(test)

find_package(OpenCL)
find_package(Threads REQUIRED)

if(NOT MSVC)
	find_package(OpenMP REQUIRED)
	find_library(JEMALLOC_LIB NAMES jemalloc)
endif()

include(cmake/chiapos.cmake)

if(OpenCL_FOUND)
	add_subdirectory(basic-opencl)
	target_compile_definitions(automy_basic_opencl
		PUBLIC CL_TARGET_OPENCL_VERSION=120
	)
endif()

if(SODIUM_FOUND)
	message(STATUS "Found libsodium")
	include_directories(${SODIUM_INCLUDE_DIR})
endif()

if(NOT MSVC AND WITH_SECP256K1)
	message(STATUS "Using secp256k1")
	include_directories(secp256k1/include)
	link_directories(secp256k1/.libs)
endif()

file(GLOB GENERATED_SOURCE_FILES
	generated/src/*.cpp
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
	src/Context.cpp
	src/Contract.cpp
	src/Challenge.cpp
	src/Solution.cpp
	src/ProofOfTime.cpp
	src/ProofOfSpace.cpp
	src/ProofOfStake.cpp
	src/ProofOfSpaceOG.cpp
	src/ProofOfSpaceNFT.cpp
	src/ProofResponse.cpp
	src/secp256k1.cpp
	src/uint128.cpp
	src/skey_t.cpp
	src/hash_t.cpp
	src/addr_t.cpp
	src/txio_key_t.cpp
	src/signature_t.cpp
	src/bls_signature_t.cpp
	src/contract/NFT.cpp
	src/contract/Data.cpp
	src/contract/WebData.cpp
	src/contract/PubKey.cpp
	src/contract/MultiSig.cpp
	src/contract/Offer.cpp
	src/contract/Token.cpp
	src/contract/TokenBase.cpp
	src/contract/TimeLock.cpp
	src/contract/PlotNFT.cpp
	src/contract/PuzzleTimeLock.cpp
	src/contract/MutableRelay.cpp
	src/contract/VirtualPlot.cpp
	src/contract/VoteSheet.cpp
	src/contract/Executable.cpp
	src/contract/method_t.cpp
	src/solution/PubKey.cpp
	src/solution/MultiSig.cpp
	src/solution/PuzzleTimeLock.cpp
	src/solution/MutableRelay.cpp
	src/solution/BLS_PubKey.cpp
	src/operation/Mint.cpp
	src/operation/Spend.cpp
	src/operation/Revoke.cpp
	src/operation/Mutate.cpp
	src/operation/Deposit.cpp
	src/operation/Execute.cpp
	src/operation/Vote.cpp
)

if(MSVC)
	include(GenerateExportHeader)
	GENERATE_EXPORT_HEADER(mmx_iface)
	set_target_properties(mmx_iface PROPERTIES ENABLE_EXPORTS 1)
	set_target_properties(mmx_iface PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)
	set_target_properties(mmx_iface PROPERTIES LINK_OPTIONS "/NODEFAULTLIB:LIBCMT")
endif()

file(GLOB GENERATED_VM_SOURCE_FILES
	generated/vm/src/*.cpp
)

add_library(mmx_vm_iface SHARED
	${GENERATED_VM_SOURCE_FILES}
	src/vm/var_t.cpp
	src/vm/varptr_t.cpp
)

add_library(mmx_vm SHARED
	src/vm/Engine.cpp
	src/vm/instr_t.cpp
	src/vm/StorageProxy.cpp
	src/vm/StorageRAM.cpp
	src/vm/StorageCache.cpp
	src/vm/StorageRocksDB.cpp
)

add_library(mmx_modules SHARED
	src/DataBase.cpp
	src/TimeLord.cpp
	src/Node.cpp
	src/Node_update.cpp
	src/Node_verify.cpp
	src/Node_validate.cpp
	src/Wallet.cpp
	src/Farmer.cpp
	src/Harvester.cpp
	src/Router.cpp
	src/WebAPI.cpp
	src/OCL_VDF.cpp
	src/TimeLordRewards.cpp
	src/vm_interface.cpp
)

add_library(uint256_t STATIC
	uint256_t/uint128_t/uint128_t.cpp
	uint256_t/uint256_t.cpp
)

target_include_directories(mmx_iface
	PUBLIC
		include
		generated/include
		generated/contract/include
		generated/operation/include
		generated/solution/include
)

target_include_directories(mmx_vm_iface
	PUBLIC
		include
		generated/vm/include
)

target_include_directories(uint256_t
	PUBLIC uint256_t uint256_t/uint128_t
)

target_compile_definitions(uint256_t PUBLIC _UINT256_T_STATIC _UINT128_T_STATIC)

target_include_directories(mmx_chiapos
	PUBLIC src/chiapos
	PRIVATE chiapos/src chiapos/lib/include
)

if(MSVC)
	set_target_properties(mmx_modules PROPERTIES ENABLE_EXPORTS 1)
	set_target_properties(mmx_modules PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)
	set_target_properties(mmx_modules PROPERTIES LINK_OPTIONS "/NODEFAULTLIB:LIBCMT")

	set_target_properties(mmx_vm_iface PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)
	set_target_properties(mmx_vm PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)
endif()

target_link_libraries(mmx_iface uint256_t secp256k1 bech32 bls vnx_base vnx_addons mmx_vm_iface)

target_link_libraries(mmx_vm_iface uint256_t vnx_base)

target_link_libraries(mmx_vm vnx_rocksdb mmx_iface)

target_link_libraries(mmx_modules vnx_rocksdb mmx_chiapos mmx_vm mmx_iface)

if(NOT MSVC)
	target_link_libraries(mmx_modules OpenMP::OpenMP_CXX)
endif()

if(OpenCL_FOUND)
	target_link_libraries(mmx_modules automy_basic_opencl)
	target_compile_definitions(mmx_modules PUBLIC WITH_OPENCL)
endif()

if(JEMALLOC_LIB)
	message(STATUS "Found jemalloc")
	target_link_libraries(mmx_modules ${JEMALLOC_LIB})
	target_compile_definitions(mmx_modules PUBLIC WITH_JEMALLOC)
endif()

add_executable(mmx src/mmx.cpp)
add_executable(mmx_node src/mmx_node.cpp)
add_executable(mmx_farmer src/mmx_farmer.cpp)
add_executable(mmx_wallet src/mmx_wallet.cpp)
add_executable(mmx_timelord src/mmx_timelord.cpp)
add_executable(mmx_harvester src/mmx_harvester.cpp)
add_executable(mmx_timelord_rewards src/mmx_timelord_rewards.cpp)
add_executable(generate_passwd src/generate_passwd.cpp)

target_link_libraries(mmx mmx_iface)
target_link_libraries(mmx_node mmx_modules)
target_link_libraries(mmx_farmer mmx_modules)
target_link_libraries(mmx_wallet mmx_modules)
target_link_libraries(mmx_timelord mmx_modules)
target_link_libraries(mmx_harvester mmx_modules)
target_link_libraries(mmx_timelord_rewards mmx_modules)
target_link_libraries(generate_passwd mmx_iface)

add_executable(tx_bench src/tx_bench.cpp)

target_link_libraries(tx_bench mmx_iface)


if (MSVC)
include(cmake/win_pack.cmake)
endif()
