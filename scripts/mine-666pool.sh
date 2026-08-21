#!/usr/bin/env bash
# Startet vds-miner gegen 666pool. Wallet als erstes Argument oder Umgebungsvariable VDS_WALLET.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
wallet="${1:-${VDS_WALLET:-}}"
worker="${2:-${VDS_WORKER:-rig1}}"
pass="${3:-x}"

if [[ -z "$wallet" ]]; then
  echo "Nutzung: $0 <VDS-WALLET> [worker] [pass]"
  echo "Beispiel: $0 VcYourAddressHere rig1"
  echo "Pool: stratum+tcp://vds.666pool.com:9338"
  exit 1
fi

bin="$root/build/vds-miner"
if [[ ! -x "$bin" ]]; then
  echo "Binary fehlt. Bitte zuerst bauen: cmake -S . -B build && cmake --build build -j"
  exit 1
fi

user="$wallet"
[[ "$wallet" == *.* ]] || user="${wallet}.${worker}"

exec "$bin" \
  -o stratum+tcp://vds.666pool.com:9338 \
  -u "$user" \
  -p "$pass" \
  --api-port 4068
