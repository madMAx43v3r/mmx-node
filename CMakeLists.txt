cmake_minimum_required(VERSION 3.11)
project(mmx-node C CXX ASM)

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(bls-signatures)

#add_subdirectory(libbech32)

find_package(Threads REQUIRED)

if(SODIUM_FOUND)
	message(STATUS "Found libsodium")
	message(STATUS "Sodium include dir = ${sodium_INCLUDE_DIR}")
	include_directories(${sodium_INCLUDE_DIR})
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fopenmp -Wall -Wno-unused-function")

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
	src/contract/Delegated.cpp
)

add_library(mmx_modules SHARED
	src/TimeLord.cpp
	src/Node.cpp
	src/Wallet.cpp
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

add_library(mmx_chiapos STATIC
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

target_link_libraries(mmx_iface uint256_t secp256k1 bls vnx_base)
target_link_libraries(mmx_modules mmx_chiapos mmx_iface)

add_executable(mmx_node src/mmx_node.cpp)
add_executable(mmx_timelord src/mmx_timelord.cpp)

add_executable(create_wallet src/create_wallet.cpp)

add_executable(test_secp256k1 test/test_secp256k1.cpp)
add_executable(test_hash_vdl test/test_hash_vdl.cpp)

target_link_libraries(mmx_node mmx_modules)
target_link_libraries(mmx_timelord mmx_modules)

target_link_libraries(create_wallet mmx_iface sodium)

target_link_libraries(test_secp256k1 mmx_iface)
target_link_libraries(test_hash_vdl mmx_iface)

