#!/bin/bash

cd $(dirname "$0")

./vnx-base/codegen.sh
./vnx-addons/codegen.sh

vnxcppcodegen --cleanup generated/ mmx interface/ modules/ vnx-base/interface/ vnx-addons/interface/ vnx-addons/modules/ interface/wallet/
vnxcppcodegen --cleanup generated/contract/ mmx.contract interface/contract/ interface/ vnx-base/interface/
vnxcppcodegen --cleanup generated/operation/ mmx.operation interface/operation/ interface/ vnx-base/interface/
vnxcppcodegen --cleanup generated/solution/ mmx.solution interface/solution/ interface/ vnx-base/interface/
