#!/bin/bash

source ./activate.sh

exec mmx_node -c "config/${NETWORK}/" config/node/ "${MMX_HOME}config/local/" "$@"
