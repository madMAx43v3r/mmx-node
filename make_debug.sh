#!/bin/bash

set -e

cd lib
./make_all.sh
cd ..

mkdir -p build

cd build

cmake -DCMAKE_BUILD_TYPE=Debug -DVNX_ADDONS_BUILD_TESTS=1 -DCMAKE_CXX_FLAGS="-fmax-errors=1" $@ ..

make -j $(nproc)

