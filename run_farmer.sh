#!/bin/bash

source ./activate.sh

mmx_farmer -c config/${NETWORK}/ config/farmer/ "${MMX_HOME}config/local/" $@
