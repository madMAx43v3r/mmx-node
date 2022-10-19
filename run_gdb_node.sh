#!/bin/bash

source ./activate.sh

gdb -ex=run --args ./build/mmx_node -c config/${NETWORK}/ config/node/ "${MMX_HOME}config/local/" $@
