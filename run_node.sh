#!/bin/bash

if [ -f NETWORK ]; then
	NETWORK=$(cat NETWORK)
else
	NETWORK=test3
	echo ${NETWORK} > NETWORK
fi

echo NETWORK=${NETWORK}

mkdir -p logs

./activate.sh

./build/mmx_node -c config/${NETWORK}/ config/local/ $@

