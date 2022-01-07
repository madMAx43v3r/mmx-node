#!/bin/bash

source ./activate.sh

./build/mmx_wallet -c config/${NETWORK}/ config/wallet/ config/local/ $@

