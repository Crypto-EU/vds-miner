#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
ver="${1:-1.0.0}"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
pkg="$stage/vds-miner"
mkdir -p "$pkg"
install -m 0755 "$root/hiveos/h-config.sh" "$pkg/h-config.sh"
install -m 0755 "$root/hiveos/h-run.sh" "$pkg/h-run.sh"
install -m 0755 "$root/hiveos/h-stats.sh" "$pkg/h-stats.sh"
install -m 0755 "$root/hiveos/h-stop.sh" "$pkg/h-stop.sh"
sed "s/^CUSTOM_VERSION=.*/CUSTOM_VERSION=${ver}/" "$root/hiveos/h-manifest.conf" > "$pkg/h-manifest.conf"
if [[ -x "$root/build/vds-miner" ]]; then
  install -m 0755 "$root/build/vds-miner" "$pkg/vds-miner"
elif [[ -x "$root/vds-miner" ]]; then
  install -m 0755 "$root/vds-miner" "$pkg/vds-miner"
fi
# Sources so HiveOS can compile on the rig if the binary is glibc-incompatible
mkdir -p "$pkg/src"
cp -a "$root/src/." "$pkg/src/"
mkdir -p "$root/dist"
out="$root/dist/vds-miner-${ver}.tar.gz"
tar -C "$stage" -czf "$out" vds-miner
echo "Wrote $out"
ls -l "$out"
