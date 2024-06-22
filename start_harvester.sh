#!/bin/bash

if pgrep -x "mmx_harvester" > /dev/null
then
    echo "Already running"
    exit
fi

DIR=$(dirname "$0")
screen -S mmx_harvester -d -m ${DIR}/run_harvester.sh

echo "Started MMX harvester in screen, to attach type 'screen -r mmx_harvester'."

