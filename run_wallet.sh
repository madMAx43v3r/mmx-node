#!/bin/bash

source ./activate.sh

mmx_wallet -c config/${NETWORK}/ config/wallet/ config/local/ $@

