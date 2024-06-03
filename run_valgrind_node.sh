#!/bin/bash

source ./activate.sh

valgrind ./build/mmx_node -c config/${NETWORK}/ config/node/ "${MMX_HOME}config/local/" $@

