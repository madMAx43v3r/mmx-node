#!/bin/bash

if [ ! -d config/local ]; then
	cp -r config/local_init config/local
	echo "Initialized config/local/ with defaults."
fi

if [ ! -f PASSWD ] || [ ! -s PASSWD ]; then
	./build/generate_passwd > PASSWD
	./build/vnx-base/tools/vnxpasswd -c config/default/ config/local/ -u mmx-admin -p $(cat PASSWD)
	echo "PASSWD=$(cat PASSWD)"
fi

chmod 600 PASSWD

if [ -f NETWORK ]; then
	NETWORK=$(cat NETWORK)
else
	NETWORK=testnet6
	echo ${NETWORK} > NETWORK
fi

echo NETWORK=${NETWORK}

export MMX_NETWORK=${NETWORK}/
export PATH=$PATH:$PWD/build:$PWD/build/exchange
