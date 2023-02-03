#!/bin/bash

export PATH=$PATH:$PWD/bin:$PWD/build:$PWD/build/tools:$PWD/build/vnx-base/tools
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/lib:$PWD/lib64:$PWD/build:$PWD/build/vnx-base:$PWD/build/vnx-addons:$PWD/build/basic-opencl

if [[ -z "${MMX_HOME}" ]]; then
	export MMX_HOME="$PWD/"
fi

rm -f "${MMX_HOME}DB_VERSION"

if [ ! -d "${MMX_HOME}config/local" ]; then
	mkdir -p "${MMX_HOME}config/"
	cp -r config/local_init "${MMX_HOME}config/local"
	echo "Initialized ${MMX_HOME}config/local/ with defaults."
fi

PASSWD_PATH="${MMX_HOME}config/local/passwd"
if [ ! -f "${PASSWD_PATH}" ] || [[ $(cat "${PASSWD_PATH}" | wc -c) -lt 64 ]]; then
	generate_passwd > "${PASSWD_PATH}"
	vnxpasswd -c config/default/ "${MMX_HOME}config/local/" -u mmx-admin -p $(cat "${PASSWD_PATH}")
	echo "PASSWD=$(cat "${PASSWD_PATH}")"
fi

chmod 600 "${PASSWD_PATH}"
cp "${PASSWD_PATH}" "${MMX_HOME}PASSWD"

if [ -f "${MMX_HOME}NETWORK" ]; then
	NETWORK=$(cat "${MMX_HOME}NETWORK")
else
	NETWORK=testnet9
	echo ${NETWORK} > "${MMX_HOME}NETWORK"
fi

if [[ -z "${MMX_DATA}" ]]; then
	MMX_DATA=${MMX_HOME}
fi

echo NETWORK=${NETWORK}

if [ "${MMX_HOME}" != "$PWD/" ]; then
	echo MMX_HOME=${MMX_HOME}
fi
if [ "${MMX_DATA}" != "${MMX_HOME}" ]; then
	echo MMX_DATA=${MMX_DATA}
fi

export MMX_NETWORK=${MMX_DATA}${NETWORK}/
