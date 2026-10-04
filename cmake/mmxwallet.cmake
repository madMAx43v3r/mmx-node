# Cargo owns the standalone wallet build. No dependency on mmx_iface, VNX,
# libsecp256k1, or any other C++ target is attached to this target.
find_program(MMX_CARGO_EXECUTABLE cargo)
find_program(MMX_RUSTC_EXECUTABLE rustc)
if(NOT MMX_CARGO_EXECUTABLE OR NOT MMX_RUSTC_EXECUTABLE)
	message(STATUS "Skipping mmxwallet: Rust and Cargo are not installed")
	return()
endif()

# rustup's executables can exist even when no toolchain is installed.
execute_process(COMMAND "${MMX_CARGO_EXECUTABLE}" --version
	RESULT_VARIABLE MMX_CARGO_STATUS OUTPUT_QUIET ERROR_QUIET TIMEOUT 10)
execute_process(COMMAND "${MMX_RUSTC_EXECUTABLE}" --version
	RESULT_VARIABLE MMX_RUSTC_STATUS OUTPUT_QUIET ERROR_QUIET TIMEOUT 10)
if(NOT "${MMX_CARGO_STATUS}" STREQUAL "0" OR NOT "${MMX_RUSTC_STATUS}" STREQUAL "0")
	message(STATUS "Skipping mmxwallet: the Rust toolchain is unavailable")
	return()
endif()

set(MMX_WALLET_SUFFIX "")
if(WIN32)
	set(MMX_WALLET_SUFFIX ".exe")
endif()
set(MMX_WALLET_CARGO_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo")
set(MMX_WALLET_PROFILE "$<IF:$<CONFIG:Debug>,debug,release>")
set(MMX_WALLET_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
	set(MMX_WALLET_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
	if(CMAKE_CONFIGURATION_TYPES)
		string(APPEND MMX_WALLET_OUTPUT_DIR "/$<CONFIG>")
	endif()
endif()
set(MMX_WALLET_JOB_SERVER)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
	set(MMX_WALLET_JOB_SERVER JOB_SERVER_AWARE TRUE)
endif()
add_custom_target(mmxwallet ALL
	COMMAND "${MMX_CARGO_EXECUTABLE}" build --locked
		--manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
		--package mmxwallet
		--target-dir "${MMX_WALLET_CARGO_DIR}"
		$<$<NOT:$<CONFIG:Debug>>:--release>
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${MMX_WALLET_OUTPUT_DIR}"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${MMX_WALLET_CARGO_DIR}/${MMX_WALLET_PROFILE}/mmxwallet${MMX_WALLET_SUFFIX}"
		"${MMX_WALLET_OUTPUT_DIR}/mmxwallet${MMX_WALLET_SUFFIX}"
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	COMMENT "Building native Rust mmxwallet"
	${MMX_WALLET_JOB_SERVER}
	VERBATIM
)
install(PROGRAMS "${MMX_WALLET_OUTPUT_DIR}/mmxwallet${MMX_WALLET_SUFFIX}" DESTINATION bin)
