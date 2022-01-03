#!/bin/bash

if [ ! -d config/local ]; then
	cp -r config/local_init config/local
	echo "Initialized config/local/ with defaults."
fi

export PATH=$PATH:$PWD/build

mkdir -p testnet3
