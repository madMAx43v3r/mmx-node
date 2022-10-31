#!/bin/bash

source ./activate.sh

./scripts/kill_high_mem.sh mmx_node 8192 &

gdb -ex=run --args ./build/mmx_node -c config/${NETWORK}/ config/node/ "${MMX_HOME}config/local/" $@

kill $(jobs -p)
