#!/bin/bash

set -e

rm -rf tmp

./scripts/reset_coverage.sh

echo "Unit tests [mmx_tests]"
./build/test/mmx_tests

echo "Unit tests [test_database]"
./build/test/test_database

echo "Unit tests [vm_engine_tests]"
./build/test/vm_engine_tests

echo "Unit tests [vm_storage_tests]"
./build/test/vm_storage_tests

./test/vm/engine_tests.sh

echo "Unit tests [vm/test_plot_nft]"
mmx_compile -e -f test/vm/test_plot_nft.js
echo Pass

