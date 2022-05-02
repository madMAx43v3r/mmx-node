
add_executable(test_engine test_engine.cpp)

target_link_libraries(test_engine mmx_vm)

if(MSVC)
	target_include_directories(test_engine PUBLIC ${MMXIFACE_BINARY_DIR})
endif()
