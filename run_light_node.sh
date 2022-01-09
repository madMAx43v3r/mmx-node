#!/bin/bash

source ./activate.sh

./build/mmx_node -c config/${NETWORK}/ config/node/ config/light_mode/ config/local/ $@

