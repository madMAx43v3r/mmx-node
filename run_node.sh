#!/bin/bash

if [ -f NETWORK ]; then
	NETWORK=$(cat NETWORK)
else
	NETWORK=test3
	mkdir testnet3
	echo ${NETWORK} > NETWORK
fi

echo NETWORK=${NETWORK}

./activate.sh

./build/mmx_node -c config/${NETWORK}/ config/local/ $@

