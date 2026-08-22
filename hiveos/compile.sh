#!/usr/bin/env bash
# Build vds-miner on the current machine (HiveOS rig or Linux packager).
# Uses vendored OpenCL headers; links against the system's libOpenCL.
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
if [[ -d "$script_dir/src/kernels" ]]; then
  # Packaged HiveOS layout: compile.sh sits next to src/
  root="$script_dir"
elif [[ -d "$script_dir/../src/kernels" ]]; then
  # Git checkout: hiveos/compile.sh
  root="$(cd "$script_dir/.." && pwd)"
else
  echo "compile.sh: src/kernels nicht gefunden neben oder ueber $script_dir" >&2
  exit 1
fi
cd "$root"

mkdir -p "$root/build/generated"
{
  printf '%s\n' '#pragma once' 'static const char kKernelSource[] = R"VDSCL('
  cat "$root/src/kernels/equihash96_5.cl"
  printf '%s\n' ')VDSCL";'
} > "$root/build/generated/kernel_embed.h"

inc=( -I "$root/src" -I "$root/build/generated" )
if [[ -d "$root/third_party/OpenCL/include" ]]; then
  inc+=( -I "$root/third_party/OpenCL/include" )
fi

libdirs=()
for d in \
  /usr/lib/x86_64-linux-gnu \
  /usr/lib64 \
  /opt/amdgpu/lib/x86_64-linux-gnu \
  /opt/rocm/lib \
  /opt/amdgpu-pro/lib/x86_64-linux-gnu \
  /opt/OpenCL/lib
 do
  [[ -d "$d" ]] && libdirs+=( -L "$d" )
done

CXX="${CXX:-g++}"
out="${1:-$root/build/vds-miner}"
mkdir -p "$(dirname "$out")"

echo "Compiling vds-miner with $CXX -> $out  (root=$root)"
"$CXX" -O3 -std=c++17 -pthread -ffast-math \
  -DCL_TARGET_OPENCL_VERSION=120 -DVDS_HAVE_OPENCL=1 \
  "${inc[@]}" \
  "$root/src/crypto.cpp" \
  "$root/src/equihash.cpp" \
  "$root/src/opencl_solver.cpp" \
  "$root/src/stratum.cpp" \
  "$root/src/api.cpp" \
  "$root/src/main.cpp" \
  "${libdirs[@]}" \
  -lOpenCL -lpthread \
  -static-libgcc -static-libstdc++ \
  -o "$out"

chmod +x "$out"
echo "Built $out"
if command -v objdump >/dev/null 2>&1; then
  echo "glibc/libstdc++ symbols:"
  objdump -T "$out" 2>/dev/null | grep -oE 'GLIBC_[0-9.]+|GLIBCXX_[0-9.]+' | sort -u || true
fi
