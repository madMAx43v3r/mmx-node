
add_executable(tx_bench tx_bench.cpp)
add_executable(dump_binary dump_binary.cpp)
add_executable(calc_test_rewards calc_test_rewards.cpp)

target_link_libraries(tx_bench mmx_iface)
target_link_libraries(dump_binary mmx_iface mmx_vm)
target_link_libraries(calc_test_rewards mmx_iface)

