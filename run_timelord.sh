#!/bin/bash

source ./activate.sh

./build/mmx_timelord -c config/${NETWORK}/ config/timelord/ config/local/ $@

