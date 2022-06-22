#!/bin/bash

if [ ! -d "${MMX_HOME}config/local" ]; then
	mkdir -p "${MMX_HOME}config/"
	cp -r config/local_init "${MMX_HOME}config/local"
	echo "Initialized ${MMX_HOME}config/local/ with defaults."
fi

if [ ! -f "${MMX_HOME}config/local/passwd" ] || [[ $(cat "${MMX_HOME}config/local/passwd" | wc -c) -lt 64 ]]; then
	./build/generate_passwd > "${MMX_HOME}config/local/passwd"
	./build/vnx-base/tools/vnxpasswd -c config/default/ "${MMX_HOME}config/local/" -u mmx-admin -p $(cat "${MMX_HOME}config/local/passwd")
fi

chmod 600 "${MMX_HOME}config/local/passwd"
cp "${MMX_HOME}config/local/passwd" "${MMX_HOME}PASSWD"
echo "PASSWD=$(cat "${MMX_HOME}PASSWD")"

if [ -f "${MMX_HOME}NETWORK" ]; then
	NETWORK=$(cat "${MMX_HOME}NETWORK")
else
	NETWORK=testnet6
	echo ${NETWORK} > "${MMX_HOME}NETWORK"
fi

echo NETWORK=${NETWORK}

if [ ! -f "${MMX_HOME}DB_VERSION" ]; then
	rm -r "${MMX_HOME}${NETWORK}/db"
	echo "Deleted old DB"
	echo "MMX1" > "${MMX_HOME}DB_VERSION"
fi

export MMX_NETWORK=${NETWORK}/
export PATH=$PATH:$PWD/build:$PWD/build/exchange
