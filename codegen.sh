#!/bin/bash

cd $(dirname "$0")

VNX_INTERFACE_DIR=${VNX_INTERFACE_DIR:-/usr/interface}

./vnx-addons/codegen.sh

vnxcppcodegen --cleanup generated/ mmx interface/ modules/ ${VNX_INTERFACE_DIR}/vnx/ vnx-addons/interface/ vnx-addons/modules/ interface/wallet/
vnxcppcodegen --cleanup generated/contract/ mmx.contract interface/contract/ interface/ ${VNX_INTERFACE_DIR}/vnx/
vnxcppcodegen --cleanup generated/operation/ mmx.operation interface/operation/ interface/ ${VNX_INTERFACE_DIR}/vnx/
vnxcppcodegen --cleanup generated/solution/ mmx.solution interface/solution/ interface/ ${VNX_INTERFACE_DIR}/vnx/
vnxcppcodegen --cleanup generated/wallet/ mmx.wallet interface/wallet/ interface/ ${VNX_INTERFACE_DIR}/vnx/
