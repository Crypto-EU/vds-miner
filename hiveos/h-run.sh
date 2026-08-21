#!/usr/bin/env bash
# HiveOS miner start script. Working dir is /hive/miners/custom/vds-miner

. /hive/miners/custom/vds-miner/h-manifest.conf 2>/dev/null || true

CUSTOM_LOG_BASENAME="${CUSTOM_LOG_BASENAME:-/var/log/miner/vds-miner/vds-miner}"
CUSTOM_CONFIG_FILENAME="${CUSTOM_CONFIG_FILENAME:-/hive/miners/custom/vds-miner/vds-miner.conf}"
mkdir -p "$(dirname "$CUSTOM_LOG_BASENAME")"

cd /hive/miners/custom/vds-miner || cd "$(dirname "$0")" || exit 1

if [[ ! -x ./vds-miner ]]; then
  echo "vds-miner binary missing — trying on-rig compile"
  if command -v g++ >/dev/null 2>&1; then
    g++ -O3 -std=c++17 -pthread -o vds-miner \
      src/crypto.cpp src/equihash.cpp src/opencl_solver.cpp src/stratum.cpp src/api.cpp src/main.cpp \
      -lOpenCL -I src 2>>"${CUSTOM_LOG_BASENAME}.log" || {
        echo "Compile failed. Install g++ and ocl-icd-opencl-dev, or copy a prebuilt vds-miner binary."
        exit 1
      }
  else
    echo "No vds-miner binary and no g++. Copy the Linux binary into this folder."
    exit 1
  fi
fi

export GPU_FORCE_64BIT_PTR=1
export GPU_MAX_HEAP_SIZE=100
export GPU_USE_SYNC_OBJECTS=1
export GPU_MAX_ALLOC_PERCENT=100
export GPU_SINGLE_ALLOC_PERCENT=100

args=""
[[ -f $CUSTOM_CONFIG_FILENAME ]] && args=$(<"$CUSTOM_CONFIG_FILENAME")

echo "Starting: ./vds-miner $args"
./vds-miner $args 2>&1 | tee -a "${CUSTOM_LOG_BASENAME}.log"
