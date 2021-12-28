#!/bin/bash

NETWORK=${NETWORK:-test1}

echo NETWORK=${NETWORK}

mkdir -p logs

./build/mmx_node -c config/${NETWORK}/ config/local/ $@

