
file(GLOB GENERATED_SOURCE_FILES
	generated/src/*.cpp
)

add_library(mmx_exchange SHARED
	${GENERATED_SOURCE_FILES}
	src/Server.cpp
	src/Client.cpp
	src/LocalTrade.cpp
	src/limit_order_t.cpp
	src/trade_order_t.cpp
)

add_executable(mmx_exch_server src/mmx_exch_server.cpp)

target_include_directories(mmx_exchange
	PUBLIC include generated/include
)

target_link_libraries(mmx_exchange mmx_iface vnx_rocksdb)

target_link_libraries(mmx_exch_server mmx_exchange)
