#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$root/build"
cmake -S "$root" -B "$root/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$root/build" -j"$(nproc)"
echo
echo "Fertig: $root/build/vds-miner"
echo "Test:   $root/build/vds-miner --self-test"
echo "GPUs:   $root/build/vds-miner --list-devices"
