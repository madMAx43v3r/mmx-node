cmake_minimum_required(VERSION 3.11)
project(mmx-node C CXX ASM)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fopenmp -Wall -Wno-unused-function")

add_subdirectory(vnx-base)
add_subdirectory(vnx-addons)
add_subdirectory(libbech32)
add_subdirectory(bls-signatures)
add_subdirectory(basic-opencl)

find_package(Threads REQUIRED)

target_compile_definitions(automy_basic_opencl
	PUBLIC CL_TARGET_OPENCL_VERSION=120
)

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
	src/ProofOfTime.cpp
	src/ProofOfSpace.cpp
	src/secp256k1.cpp
	src/contract/PubKey.cpp
)

add_library(mmx_modules SHARED
	src/TimeLord.cpp
	src/Node.cpp
	src/Wallet.cpp
	src/Farmer.cpp
	src/Harvester.cpp
	src/Router.cpp
	src/OCL_VDF.cpp
)

add_library(uint256_t STATIC
	uint256_t/uint128_t.cpp
	uint256_t/uint256_t.cpp
)

set(BLAKE3_PATH ${CMAKE_CURRENT_SOURCE_DIR}/chiapos/src/b3)
set(FSE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/chiapos/lib/FiniteStateEntropy/lib)

IF (WIN32)
set(BLAKE3_SRC
	${BLAKE3_PATH}/blake3.c
	${BLAKE3_PATH}/blake3_portable.c
	${BLAKE3_PATH}/blake3_dispatch.c
	${BLAKE3_PATH}/blake3_avx2.c
	${BLAKE3_PATH}/blake3_avx512.c
	${BLAKE3_PATH}/blake3_sse41.c
)
ELSEIF(OSX_NATIVE_ARCHITECTURE STREQUAL "arm64")
set(BLAKE3_SRC
	${BLAKE3_PATH}/blake3.c
	${BLAKE3_PATH}/blake3_portable.c
	${BLAKE3_PATH}/blake3_dispatch.c
)
ELSE()
set(BLAKE3_SRC
	${BLAKE3_PATH}/blake3.c
	${BLAKE3_PATH}/blake3_portable.c
	${BLAKE3_PATH}/blake3_dispatch.c
	${BLAKE3_PATH}/blake3_avx2_x86-64_unix.S
	${BLAKE3_PATH}/blake3_avx512_x86-64_unix.S
	${BLAKE3_PATH}/blake3_sse41_x86-64_unix.S
)
ENDIF()

add_library(mmx_chiapos SHARED
	src/chiapos/chiapos.cpp
	chiapos/src/chacha8.c
	${BLAKE3_SRC}
	${FSE_PATH}/fse_compress.c
	${FSE_PATH}/fse_decompress.c
	${FSE_PATH}/entropy_common.c
	${FSE_PATH}/hist.c
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
target_link_libraries(mmx_modules automy_basic_opencl mmx_chiapos mmx_iface)

add_executable(mmx src/mmx.cpp)
add_executable(mmx_node src/mmx_node.cpp)
add_executable(mmx_timelord src/mmx_timelord.cpp)

target_link_libraries(mmx mmx_iface)
target_link_libraries(mmx_node mmx_modules)
target_link_libraries(mmx_timelord mmx_modules)

add_executable(tx_bench src/tx_bench.cpp)

target_link_libraries(tx_bench mmx_iface)

