
add_executable(test_engine test_engine.cpp)
add_executable(create_test_contract create_test_contract.cpp)

target_link_libraries(test_engine mmx_vm)
target_link_libraries(create_test_contract mmx_vm)

if(MSVC)
	target_include_directories(test_engine PUBLIC ${MMXIFACE_BINARY_DIR})
	target_include_directories(create_test_contract PUBLIC ${MMXIFACE_BINARY_DIR})
	target_include_directories(create_test_contract2 PUBLIC ${MMXIFACE_BINARY_DIR})
endif()
