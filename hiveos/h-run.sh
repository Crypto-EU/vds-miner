#!/usr/bin/env bash
# HiveOS miner start script. Working dir is /hive/miners/custom/vds-miner

. /hive/miners/custom/vds-miner/h-manifest.conf 2>/dev/null || true

CUSTOM_LOG_BASENAME="${CUSTOM_LOG_BASENAME:-/var/log/miner/vds-miner/vds-miner}"
CUSTOM_CONFIG_FILENAME="${CUSTOM_CONFIG_FILENAME:-/hive/miners/custom/vds-miner/vds-miner.conf}"
mkdir -p "$(dirname "$CUSTOM_LOG_BASENAME")"
log="${CUSTOM_LOG_BASENAME}.log"

cd /hive/miners/custom/vds-miner || cd "$(dirname "$0")" || exit 1
chmod +x compile.sh 2>/dev/null || true

binary_ok() {
  [[ -x ./vds-miner ]] || return 1
  # Incompatible glibc/libstdc++ prints to stderr and exits 127 before main().
  local err
  err=$(./vds-miner -h 2>&1 >/dev/null) || true
  if echo "$err" | grep -qE 'GLIBC_|GLIBCXX_|not found'; then
    return 1
  fi
  ./vds-miner -h >/dev/null 2>&1
}

ensure_cxx() {
  command -v g++ >/dev/null 2>&1 && return 0
  echo "g++ fehlt — versuche Installation (HiveOS)..." | tee -a "$log"
  if command -v apt-get >/dev/null 2>&1; then
    apt-get update -qq >>"$log" 2>&1 || true
    DEBIAN_FRONTEND=noninteractive apt-get install -y g++ >>"$log" 2>&1 || true
  fi
  command -v g++ >/dev/null 2>&1
}

if ! binary_ok; then
  echo "vds-miner Binary fehlt oder passt nicht zu dieser glibc — kompiliere auf dem Rig" | tee -a "$log"
  if [[ -x ./vds-miner ]]; then
    mv -f ./vds-miner "./vds-miner.glibc-mismatch" 2>/dev/null || rm -f ./vds-miner
  fi
  if ! ensure_cxx; then
    echo "Kein g++. Auf dem Rig: apt-get install -y g++" | tee -a "$log"
    echo "Besser: Paket 1.1.6+ mit glibc-2.17-Binary verwenden (kein Kompilieren noetig)." | tee -a "$log"
    exit 1
  fi
  echo "compile.sh startet in $(pwd)" | tee -a "$log"
  if ! ./compile.sh ./vds-miner >"$log.compile" 2>&1; then
    echo "Compile failed. Ausgabe:" | tee -a "$log"
    cat "$log.compile" | tee -a "$log"
    echo "Compile failed. Log: $log" | tee -a "$log"
    exit 1
  fi
  cat "$log.compile" >>"$log"
  if ! binary_ok; then
    echo "Neu gebaute Binary startet nicht. Siehe $log" | tee -a "$log"
    ./vds-miner -h 2>&1 | tee -a "$log" || true
    exit 1
  fi
  echo "On-rig compile OK" | tee -a "$log"
fi

export GPU_FORCE_64BIT_PTR=1
export GPU_MAX_HEAP_SIZE=100
export GPU_USE_SYNC_OBJECTS=1
export GPU_MAX_ALLOC_PERCENT=100
export GPU_SINGLE_ALLOC_PERCENT=100

args=""
[[ -f $CUSTOM_CONFIG_FILENAME ]] && args=$(<"$CUSTOM_CONFIG_FILENAME")

echo "Starting: ./vds-miner $args"
./vds-miner $args 2>&1 | tee -a "$log"
