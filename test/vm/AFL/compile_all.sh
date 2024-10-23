#!/bin/bash

mmx_compile -f ../compiler_tests.js -o inputs/compiler_tests.js.dat
mmx_compile -f ../engine_tests.js -o inputs/engine_tests.js.dat

for file in ../fails/*.js; do
        echo "Compiling [$file]"
        mmx_compile -f $file -o inputs/${file}.dat
done

mv fails/*.dat inputs/
rmdir fails

