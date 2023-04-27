#!/bin/bash

set -e

echo "Unit tests [mmx_tests]"
./build/test/mmx_tests
echo "Pass"

echo "Unit tests [vm_engine_tests]"
./build/test/vm_engine_tests
echo "Pass"

./test/vm/engine_tests.sh
