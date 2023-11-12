#!/bin/bash

mkdir -p build

cd build

cmake -DCMAKE_BUILD_TYPE=Debug -DWITH_COVERAGE=1 -DCMAKE_CXX_FLAGS="-fmax-errors=1" $@ ..

make -j8

cd ..

./scripts/reset_coverage.sh
