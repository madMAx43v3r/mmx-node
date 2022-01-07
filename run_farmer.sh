#!/bin/bash

./activate.sh

./build/mmx_farmer -c config/default/ config/farmer/ config/local/ $@

