#!/bin/bash

cd $(dirname "$0")

VNX_INTERFACE_DIR=${VNX_INTERFACE_DIR:-/usr/interface}

vnxcppcodegen --cleanup generated/ mmx interface/ ${VNX_INTERFACE_DIR}/vnx/
