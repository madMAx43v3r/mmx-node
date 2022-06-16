#!/bin/bash

source ./activate.sh

mmx_wallet -c config/${NETWORK}/ config/wallet/ "${MMX_HOME}config/local/" $@
