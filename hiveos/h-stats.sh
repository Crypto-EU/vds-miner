#!/usr/bin/env bash
# HiveOS stats callback. Sets $khs and $stats.

. /hive/miners/custom/vds-miner/h-manifest.conf 2>/dev/null || true
API_PORT="${MINER_API_PORT:-4068}"

khs=0
stats=

js=$(curl -sS --connect-timeout 2 "http://127.0.0.1:${API_PORT}/" 2>/dev/null)
if [[ -z "$js" ]]; then
  khs=0
  stats='{"hs":[0],"hs_units":"hs","uptime":0,"ver":"1.0.0","ar":[0,0],"algo":"equihash96_5"}'
  return 0
fi

# Sol/s from JSON "sols" (Equihash solutions per second). Hive khs = Sol/s / 1000.
khs=$(echo "$js" | sed -n 's/.*"khs":\([0-9.]*\).*/\1/p' | head -1)
[[ -z "$khs" ]] && khs=$(echo "$js" | sed -n 's/.*"sols":\([0-9.]*\).*/\1/p' | head -1 | awk '{printf "%.6f", $1/1000}')
[[ -z "$khs" ]] && khs=0
stats="$js"
