#!/bin/bash

./activate.sh

./build/mmx_harvester -c config/default/ config/harvester/ config/local/ $@

