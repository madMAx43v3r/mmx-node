#!/bin/bash

if pgrep -x "mmx_wallet" > /dev/null
then
    echo "Already running"
    exit
fi

DIR=$(dirname "$0")
screen -S mmx_wallet -d -m ${DIR}/run_wallet.sh

echo "Started MMX wallet in screen, to attach type 'screen -r mmx_wallet'."

