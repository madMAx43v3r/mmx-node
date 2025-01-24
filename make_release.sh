#!/bin/bash

set -e

DIST=${1:-dist}

cd lib
./make_all.sh
cd ..

mkdir -p build

cd build

rm -rf $DIST

cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$DIST $@ ..

make -j $(nproc) install

rm -rf $DIST/share/mmx-node/config/local
