#!/bin/bash

pm2 start index.js --name mmx-rpc --namespace mmx --cron-restart="0 0 * * *"
