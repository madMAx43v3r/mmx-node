
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

add_library(mmx_chiapos SHARED
	src/chiapos/chiapos.cpp
	chiapos/src/chacha8.c
	${BLAKE3_SRC}
	${FSE_PATH}/fse_compress.c
	${FSE_PATH}/fse_decompress.c
	${FSE_PATH}/entropy_common.c
	${FSE_PATH}/hist.c
)
