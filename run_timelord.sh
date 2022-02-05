#!/bin/bash

source ./activate.sh

mmx_timelord -c config/${NETWORK}/ config/timelord/ config/local/ $@

