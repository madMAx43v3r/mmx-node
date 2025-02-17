#!/bin/bash

set -e

MAX_FORK_LEN=0
HEIGHT=$(mmx node get header | jq .height)

for((i = 0; i <= HEIGHT; i++)); do
    FORK_LEN=$(mmx node get header $i | jq .space_fork_len)
    if [ $FORK_LEN -gt $MAX_FORK_LEN ]; then
        MAX_FORK_LEN=$FORK_LEN
    fi
    echo "Block $i: $FORK_LEN"
done

echo "Max space fork length: $MAX_FORK_LEN"
