cmake_minimum_required(VERSION 3.10)
project(mmx-node C CXX)

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(bls-signatures)

#add_subdirectory(libbech32)

find_package(Threads REQUIRED)

if(APPLE)
	find_package(sodium REQUIRED)
endif()

if(SODIUM_FOUND)
	message(STATUS "Found libsodium")
	message(STATUS "Sodium include dir = ${sodium_INCLUDE_DIR}")
	include_directories(${sodium_INCLUDE_DIR})
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-function")

file(GLOB GENERATED_SOURCE_FILES generated/src/*.cpp)

add_library(mmx_iface SHARED
	${GENERATED_SOURCE_FILES}
)

target_include_directories(mmx_iface
	PUBLIC
		include
		generated/include
)

target_link_libraries(mmx_iface bls)

