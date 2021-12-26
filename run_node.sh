#!/bin/bash

NETWORK=${1:-test1}

echo NETWORK=${NETWORK}

./build/mmx_node -c config/${NETWORK}/ config/local/

