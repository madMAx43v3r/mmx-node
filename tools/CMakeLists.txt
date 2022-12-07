
add_executable(tx_bench tx_bench.cpp)
add_executable(calc_test_rewards calc_test_rewards.cpp)

target_link_libraries(tx_bench mmx_iface)
target_link_libraries(calc_test_rewards mmx_iface)

