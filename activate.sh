#!/bin/bash

if [ ! -d config/local ]; then
	cp -r config/local_init config/local
	echo "Initialized config/local/ with defaults."
fi

if [ ! -f config/local/passwd ] || [[ $(cat config/local/passwd | wc -c) -lt 64 ]]; then
	./build/generate_passwd > config/local/passwd
	./build/vnx-base/tools/vnxpasswd -c config/default/ config/local/ -u mmx-admin -p $(cat config/local/passwd)
fi

chmod 600 config/local/passwd
cp config/local/passwd PASSWD
echo "PASSWD=$(cat PASSWD)"

if [ -f NETWORK ]; then
	NETWORK=$(cat NETWORK)
else
	NETWORK=testnet6
	echo ${NETWORK} > NETWORK
fi

echo NETWORK=${NETWORK}

export MMX_NETWORK=${NETWORK}/
export PATH=$PATH:$PWD/build:$PWD/build/exchange
