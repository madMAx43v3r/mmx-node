#!/bin/bash

set -e

mkdir -p .cache

cd secp256k1

SECP_COMMIT=$(git rev-parse HEAD)

if [[ -f ../.cache/SECP_BUILD && -f .libs/libsecp256k1.so ]]; then
	SECP_BUILD=$(cat ../.cache/SECP_BUILD || true)
fi

if [ "${SECP_COMMIT}" != "${SECP_BUILD}" ]
then
	echo "Compiling secp256k1 ${SECP_COMMIT}"
	./autogen.sh > /dev/null
	./configure > /dev/null
	make clean > /dev/null
	make -j8
	echo -n ${SECP_COMMIT} > ../.cache/SECP_BUILD
fi

cd ..
