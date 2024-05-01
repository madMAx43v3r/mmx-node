#!/bin/bash

lcov --directory build/ --capture --output-file coverage.info

genhtml --demangle-cpp -o build/coverage coverage.info

