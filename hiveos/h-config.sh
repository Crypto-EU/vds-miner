#!/usr/bin/env bash
# HiveOS: generate miner command line from the flight sheet.

[[ -z $CUSTOM_TEMPLATE ]] && echo "ERROR: no wallet template" && return 1

CUSTOM_URL="${CUSTOM_URL:-stratum+tcp://vds.666pool.com:9338}"
# Flight sheets sometimes pass host:port without scheme
if [[ "$CUSTOM_URL" != *"://"* ]]; then
  CUSTOM_URL="stratum+tcp://${CUSTOM_URL}"
fi

conf="-o ${CUSTOM_URL} -u ${CUSTOM_TEMPLATE}"
[[ -n $CUSTOM_PASS ]] && conf+=" -p ${CUSTOM_PASS}" || conf+=" -p x"
conf+=" --api-port ${MINER_API_PORT:-4068}"
[[ -n $CUSTOM_USER_CONFIG ]] && conf+=" ${CUSTOM_USER_CONFIG}"

echo "$conf" > "${CUSTOM_CONFIG_FILENAME}"
echo "vds-miner config: $conf"
