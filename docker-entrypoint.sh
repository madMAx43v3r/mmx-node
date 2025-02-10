#!/bin/bash

source ./activate.sh

config_path="${MMX_HOME}config/local/"

if [ "${MMX_NETWORK}" ]; then
  echo "${MMX_NETWORK}" > "${MMX_HOME}NETWORK"
fi

if [[ "${MMX_ALLOW_REMOTE}" == "true" ]]; then
  echo true > "${config_path}allow_remote" && echo "*** Remote Access Enabled ***"
elif [[ "${MMX_ALLOW_REMOTE}" == "false" ]]; then
  echo false > "${config_path}allow_remote" && echo "*** Remote Access Disabled ***"
fi

if [[ "${MMX_HARVESTER_ENABLED}" == "true" ]]; then
  echo true > "${config_path}harvester" && echo "*** Harvester Enabled ***"
elif [[ "${MMX_HARVESTER_ENABLED}" == "false" ]]; then
  echo false > "${config_path}harvester" && echo "*** Harvester Disabled ***"
fi

if [[ "${MMX_FARMER_ENABLED}" == "true" ]]; then
  echo true > "${config_path}farmer" && echo "*** Farmer Enabled ***"
elif [[ "${MMX_FARMER_ENABLED}" == "false" ]]; then
  echo false > "${config_path}farmer" && echo "*** Farmer Disabled ***"
fi

exec "$@"
