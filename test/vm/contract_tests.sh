#!/bin/bash

set -e

for file in test/vm/test_*.js; do
	echo "Unit tests [$file]"
	./build/tools/mmx_compile -e -f $file
	echo "Pass"
done
