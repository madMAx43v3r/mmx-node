#!/bin/bash

git rm -rf bls-signatures libbech32 secp256k1

git pull && git submodule update --init --recursive && ./make_devel.sh

./activate.sh

