#!/bin/bash

source ./activate.sh

mmx_exch_server -c config/${NETWORK}/ "${MMX_HOME}config/local/"
