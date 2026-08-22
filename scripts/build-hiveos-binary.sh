#!/usr/bin/env bash
# Build a HiveOS-compatible vds-miner (glibc 2.17) inside /opt/bionic.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
chroot="${BIONIC_CHROOT:-/opt/bionic}"
if [[ ! -x "$chroot/usr/bin/g++-7" ]]; then
  echo "Need Ubuntu 18.04 chroot with g++-7 at $chroot" >&2
  exit 1
fi
if [[ -f "$root/third_party/OpenCL/stub.c" ]]; then
  sudo cp "$root/third_party/OpenCL/stub.c" "$chroot/tmp/opencl_stub.c"
  sudo chroot "$chroot" /bin/bash -lc 'gcc -shared -fPIC -O2 -o /usr/lib/x86_64-linux-gnu/libOpenCL.so.1 /tmp/opencl_stub.c'
fi
sudo chroot "$chroot" /bin/bash -lc "export PATH=/usr/bin:/bin; cd /workspace && CXX=g++-7 ./hiveos/compile.sh /workspace/build/vds-miner"
echo "HiveOS binary: $root/build/vds-miner"
objdump -T "$root/build/vds-miner" | grep -oE 'GLIBC_[0-9.]+' | sort -u
