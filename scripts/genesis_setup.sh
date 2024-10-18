#!/bin/bash

set -e

./build/tools/mmx_compile -n $1 -f src/contract/swap.js -t -o data/tx_swap_binary.dat > config/default/chain/params/swap_binary
./build/tools/mmx_compile -n $1 -f src/contract/offer.js -t -o data/tx_offer_binary.dat > config/default/chain/params/offer_binary
./build/tools/mmx_compile -n $1 -f src/contract/token.js -t -o data/tx_token_binary.dat > config/default/chain/params/token_binary
./build/tools/mmx_compile -n $1 -f src/contract/virtual_plot.js -t -o data/tx_plot_binary.dat > config/default/chain/params/plot_binary
./build/tools/mmx_compile -n $1 -f src/contract/plot_nft.js -t -o data/tx_plot_nft_binary.dat > config/default/chain/params/plot_nft_binary
./build/tools/mmx_compile -n $1 -f src/contract/nft.js -t -o data/tx_nft_binary.dat > config/default/chain/params/nft_binary
./build/tools/mmx_compile -n $1 -f src/contract/template.js -t -o data/tx_template_binary.dat > config/default/chain/params/template_binary

echo OK
