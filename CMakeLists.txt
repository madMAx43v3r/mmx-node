cmake_minimum_required(VERSION 3.11)
project(mmx-node C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fopenmp -Wall -Wno-unused-function")

set(ARITH "easy" CACHE STRING "BLS ARITH")
set(BUILD_BLS_PYTHON_BINDINGS OFF CACHE BOOL "BUILD_BLS_PYTHON_BINDINGS")
set(BUILD_BLS_TESTS OFF CACHE BOOL "BUILD_BLS_TESTS")
set(BUILD_BLS_BENCHMARKS OFF CACHE BOOL "BUILD_BLS_BENCHMARKS")
set(LIBBECH32_BUILD_GOOGLETEST OFF CACHE BOOL "LIBBECH32_BUILD_GOOGLETEST")
set(LIBBECH32_BUILD_RAPIDCHECK OFF CACHE BOOL "LIBBECH32_BUILD_RAPIDCHECK")

add_subdirectory(vnx-base)
add_subdirectory(vnx-addons)
add_subdirectory(vnx-rocksdb)
add_subdirectory(libbech32)
add_subdirectory(bls-signatures)
add_subdirectory(exchange)

find_package(OpenCL)
find_package(Threads REQUIRED)

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

if(WITH_SECP256K1)
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
	src/Contract.cpp
	src/Solution.cpp
	src/ProofOfTime.cpp
	src/ProofOfSpace.cpp
	src/secp256k1.cpp
	src/hash_t.cpp
	src/contract/NFT.cpp
	src/contract/Data.cpp
	src/contract/WebData.cpp
	src/contract/PubKey.cpp
	src/contract/MultiSig.cpp
	src/contract/Token.cpp
	src/contract/Staking.cpp
	src/contract/Condition.cpp
	src/solution/PubKey.cpp
	src/solution/MultiSig.cpp
	src/operation/Mint.cpp
	src/operation/Spend.cpp
	src/operation/Mutate.cpp
)

add_library(mmx_modules SHARED
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
)

add_library(uint256_t STATIC
	uint256_t/uint128_t.cpp
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

target_include_directories(uint256_t PUBLIC uint256_t)

target_include_directories(mmx_chiapos
	PUBLIC src/chiapos
	PRIVATE chiapos/src chiapos/lib/include
)

target_link_libraries(mmx_iface uint256_t secp256k1 bech32 bls vnx_addons vnx_base)
target_link_libraries(mmx_modules vnx_rocksdb mmx_chiapos mmx_iface)

if(OpenCL_FOUND)
	target_link_libraries(mmx_modules automy_basic_opencl)
	target_compile_definitions(mmx_modules PUBLIC WITH_OPENCL)
endif()

add_executable(mmx src/mmx.cpp)
add_executable(mmx_node src/mmx_node.cpp)
add_executable(mmx_farmer src/mmx_farmer.cpp)
add_executable(mmx_wallet src/mmx_wallet.cpp)
add_executable(mmx_timelord src/mmx_timelord.cpp)
add_executable(mmx_harvester src/mmx_harvester.cpp)

target_link_libraries(mmx mmx_iface mmx_exchange)
target_link_libraries(mmx_node mmx_modules mmx_exchange)
target_link_libraries(mmx_farmer mmx_modules)
target_link_libraries(mmx_wallet mmx_modules)
target_link_libraries(mmx_timelord mmx_modules)
target_link_libraries(mmx_harvester mmx_modules)

add_executable(tx_bench src/tx_bench.cpp)

target_link_libraries(tx_bench mmx_iface)

