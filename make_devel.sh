#!/bin/bash

mkdir -p build

cd build

cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS="-fmax-errors=1" $@ ..

make -j8

