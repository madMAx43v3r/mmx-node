#!/bin/bash

if pgrep -x "mmx_node" > /dev/null
then
    echo "Already running"
    exit
fi

DIR=$(dirname "$0")
screen -S mmx_node -d -m ${DIR}/run_node.sh

echo "Started MMX node in screen, to attach type 'screen -r mmx_node'."

