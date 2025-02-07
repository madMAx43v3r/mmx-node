#!/bin/bash

set -e

export PATH=$PATH:$PWD/bin:$PWD/build:$PWD/build/tools:$PWD/build/vnx-base/tools:/usr/share/mmx-node/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/lib:$PWD/lib64:$PWD/build:$PWD/build/vnx-base:$PWD/build/vnx-addons:$PWD/build/basic-opencl

if [[ -z "${MMX_HOME}" ]]; then
	export MMX_HOME="$PWD/"
else
	mkdir -p ${MMX_HOME}
	echo MMX_HOME=${MMX_HOME}
fi

mkdir -p ${MMX_HOME}config/local
if cp --update=none /dev/null /dev/null 2> /dev/null; then
	cp -rv --update=none config/local_init/. ${MMX_HOME}config/local
else
	cp -rnv config/local_init/. ${MMX_HOME}config/local
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
	NETWORK=mainnet
	echo ${NETWORK} > "${MMX_HOME}NETWORK"
fi

if [ -f "${MMX_HOME}config/local/MMX_DATA" ]; then
	MMX_DATA=$(cat "${MMX_HOME}config/local/MMX_DATA")
fi
if [[ -z "${MMX_DATA}" ]]; then
	MMX_DATA=${MMX_HOME}
fi

echo NETWORK=${NETWORK}

if [ "${MMX_DATA}" != "${MMX_HOME}" ]; then
	echo MMX_DATA=${MMX_DATA}
fi

export MMX_NETWORK=${MMX_DATA}${NETWORK}/

set +e
