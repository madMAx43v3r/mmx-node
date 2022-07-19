#!/bin/bash

cd lib/isa-l_crypto
make -j8 -f Makefile.unx
cd ../..

mkdir -p build

cd build

cmake -DCMAKE_BUILD_TYPE=Release $@ ..

make -j8

