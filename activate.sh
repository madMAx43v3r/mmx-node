#!/bin/bash

if [[ -z "${MMX_HOME}" ]]; then
	MMX_HOME="$(pwd)/"
	export MMX_HOME=${MMX_HOME}
fi

rm "${MMX_HOME}DB_VERSION"

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
	NETWORK=testnet8
	echo ${NETWORK} > "${MMX_HOME}NETWORK"
fi

echo NETWORK=${NETWORK}

export MMX_NETWORK=${NETWORK}/
export PATH=$PATH:$PWD/build:$PWD/build/exchange
