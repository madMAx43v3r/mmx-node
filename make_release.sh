#!/bin/bash

set -e

cd lib
./make_all.sh
cd ..

mkdir -p build

cd build

rm -rf dist

cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=dist $@ ..

make -j $(nproc) install

rm -r dist/config/local || true
