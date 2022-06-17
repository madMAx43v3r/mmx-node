#!/bin/bash

source ./activate.sh

mmx_harvester -c config/${NETWORK}/ config/farmer/ "${MMX_HOME}config/local/" $@
