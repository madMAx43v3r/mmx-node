#!/bin/bash

source ./activate.sh

mmx_farmer -c config/${NETWORK}/ config/farmer/ config/local/ $@

