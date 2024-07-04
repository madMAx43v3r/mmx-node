#!/bin/bash

set -e

ulimit -n 100000

node index.js
