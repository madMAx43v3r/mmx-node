cmake_minimum_required(VERSION 3.11)
project(mmx-node C CXX)

set(CMAKE_CXX_STANDARD 11)

include(FetchContent)

FetchContent_Declare(
	secp256k1
	GIT_REPOSITORY https://github.com/bitcoin-core/secp256k1.git
	GIT_TAG        fecf436d5327717801da84beb3066f5a9b80ea8e
)

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
)

add_library(mmx_modules SHARED
	src/TimeLord.cpp
	src/Node.cpp
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

target_link_libraries(mmx_iface uint256_t secp256k1 bls vnx_base)
target_link_libraries(mmx_modules mmx_iface)

add_executable(mmx_timelord src/mmx_timelord.cpp)

add_executable(test_secp256k1 test/test_secp256k1.cpp)
add_executable(test_hash_vdl test/test_hash_vdl.cpp)

target_link_libraries(mmx_timelord mmx_modules)

target_link_libraries(test_secp256k1 mmx_iface)
target_link_libraries(test_hash_vdl mmx_iface)

