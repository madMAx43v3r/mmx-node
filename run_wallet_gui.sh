#!/bin/bash

source ./activate.sh

NODE=$(cat "${MMX_HOME}config/local/node")

NODE=$(zenity --entry --title="Remote Node Address" --text="Node IP or hostname:" --entry-text="$NODE")

echo "$NODE" > "${MMX_HOME}config/local/node"

mmx_wallet -c config/${NETWORK}/ config/wallet/ "${MMX_HOME}config/local/" --gui $@
