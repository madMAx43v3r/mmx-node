#!/bin/bash

set -e

ulimit -n 100000

sudo node index.js
