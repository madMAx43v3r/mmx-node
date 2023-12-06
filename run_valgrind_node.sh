#!/bin/bash

source ./activate.sh

valgrind --tool=callgrind ./build/mmx_node -c config/${NETWORK}/ config/node/ "${MMX_HOME}config/local/" $@

