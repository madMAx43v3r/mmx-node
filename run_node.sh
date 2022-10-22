#!/bin/bash

source ./activate.sh

mmx_node -c config/${NETWORK}/ config/node/ "${MMX_HOME}config/local/" $@
