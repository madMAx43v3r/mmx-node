#!/bin/bash

set -e

echo "Unit tests [test/vm/engine_tests.js]"
./build/tools/mmx_compile -e -f test/vm/engine_tests.js
echo "Pass"

echo "Unit tests [test/vm/compiler_tests.js]"
./build/tools/mmx_compile -e -f test/vm/compiler_tests.js
echo "Pass"

for file in test/vm/fails/*.js; do
	echo "Asserting fail on [$file]"
	./build/tools/mmx_compile -e -w -g 10000000 --assert-fail -f $file
	echo "Pass"
done
