#!/bin/bash

./activate.sh

./build/mmx_timelord -c config/default/ config/timelord/ config/local/ $@

