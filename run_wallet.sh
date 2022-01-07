#!/bin/bash

./activate.sh

./build/mmx_wallet -c config/default/ config/wallet/ config/local/ $@

