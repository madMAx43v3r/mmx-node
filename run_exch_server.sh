#!/bin/bash

source ./activate.sh

./build/exchange/mmx_exch_server -c config/${NETWORK}/ config/local/
