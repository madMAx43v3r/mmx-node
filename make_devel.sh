#!/bin/bash

set -e

cd lib
./make_all.sh
cd ..

mkdir -p build

cd build

cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVNX_ADDONS_BUILD_TESTS=1 -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer -fmax-errors=1" $@ ..

make -j $(nproc)

