#!/bin/bash

NETWORK=${NETWORK:-test1}

echo NETWORK=${NETWORK}

mkdir -p logs

./activate.sh

./build/mmx_node -c config/${NETWORK}/ config/local/ $@

