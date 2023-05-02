#!/bin/bash

mkdir -p build

cd build

rm -r dist

cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=dist $@ ..

make -j16 install

rm -r dist/config/local || true
