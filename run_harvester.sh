#!/bin/bash

source ./activate.sh

./build/mmx_harvester -c config/${NETWORK}/ config/farmer/ config/local/ $@

