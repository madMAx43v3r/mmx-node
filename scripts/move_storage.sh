#!/bin/bash

if [[ -z "$1" ]]; then
	echo "Usage: move_storage.sh destination_path/"
	exit
fi

if pgrep -x "mmx_node" > /dev/null
then
    echo "Please stop MMX node first."
    exit
fi

set -e
source ./activate.sh

CURR="${PWD}"
mkdir -p "$1"
cd "$1"
DST="${PWD}/"
cd "${CURR}"

echo "SRC=${MMX_NETWORK}"
echo "DST=${DST}"

echo "Copying files ..."
cp -rv "${MMX_NETWORK}" "${DST}" || exit

echo "${DST}" > config/local/MMX_DATA

echo "Deleting old files ..."
rm -r "${MMX_NETWORK}"

echo "Done"

