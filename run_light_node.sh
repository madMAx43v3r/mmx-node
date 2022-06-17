#!/bin/bash

source ./activate.sh

mmx_node -c config/${NETWORK}/ config/node/ config/light_mode/ "${MMX_HOME}config/local/" $@
