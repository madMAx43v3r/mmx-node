#!/bin/bash

set -e

git pull && git submodule update --init --recursive && ./make_devel.sh

./activate.sh

