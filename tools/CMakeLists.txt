
add_executable(tx_bench tx_bench.cpp)
add_executable(mmx_compile mmx_compile.cpp)
add_executable(mmx_postool mmx_postool.cpp)
add_executable(mmx_posbench mmx_posbench.cpp)
add_executable(dump_table dump_table.cpp)
add_executable(dump_binary dump_binary.cpp)
add_executable(generate_passwd generate_passwd.cpp)
add_executable(calc_test_rewards calc_test_rewards.cpp)

target_link_libraries(tx_bench mmx_iface)
target_link_libraries(mmx_compile mmx_iface mmx_vm)
target_link_libraries(mmx_postool mmx_iface mmx_pos)
target_link_libraries(mmx_posbench mmx_iface mmx_pos)
target_link_libraries(dump_table mmx_iface mmx_db)
target_link_libraries(dump_binary mmx_iface mmx_vm)
target_link_libraries(generate_passwd mmx_iface)
target_link_libraries(calc_test_rewards mmx_iface)

install(TARGETS
	mmx_compile mmx_postool mmx_posbench
	DESTINATION bin
)
install(TARGETS
	generate_passwd dump_binary tx_bench
	DESTINATION ${NODE_INSTALL_PATH}/bin
)
