#!/bin/bash

if [ ! -d "${MMX_HOME}config/local" ]; then
	mkdir -p "${MMX_HOME}config/"
	cp -r config/local_init "${MMX_HOME}config/local"
	echo "Initialized ${MMX_HOME}config/local/ with defaults."
fi

PASSWD_PATH="${MMX_HOME}config/local/passwd"
if [ ! -f "${PASSWD_PATH}" ] || [[ $(cat "${PASSWD_PATH}" | wc -c) -lt 64 ]]; then
	./build/generate_passwd > "${PASSWD_PATH}"
	./build/vnx-base/tools/vnxpasswd -c config/default/ "${MMX_HOME}config/local/" -u mmx-admin -p $(cat "${PASSWD_PATH}")
	echo "PASSWD=$(cat "${PASSWD_PATH}")"
fi

chmod 600 "${PASSWD_PATH}"
cp "${PASSWD_PATH}" "${MMX_HOME}PASSWD"

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
