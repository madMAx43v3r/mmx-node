#!/bin/bash

source ./activate.sh

./build/mmx_farmer -c config/${NETWORK}/ config/farmer/ config/local/ $@

