#!/bin/bash

set -e

./scripts/reset_coverage.sh

echo "Unit tests [mmx_tests]"
./build/test/mmx_tests

echo "Unit tests [vm_engine_tests]"
./build/test/vm_engine_tests

./test/vm/engine_tests.sh
