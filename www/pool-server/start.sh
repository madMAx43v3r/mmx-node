#!/bin/bash

THREADS=${1:-max}

pm2 start server.js  --name pool-server  --namespace mmx-pool -i $THREADS
pm2 start verify.js  --name pool-verify  --namespace mmx-pool
pm2 start account.js --name pool-account --namespace mmx-pool
