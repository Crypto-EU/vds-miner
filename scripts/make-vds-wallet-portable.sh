#!/usr/bin/env bash
# Build dist/VDS-Wallet.zip — original Windows wallet in its own folder.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
zip_src="$root/wallet/Wallet.zip"
out="$root/dist/VDS-Wallet.zip"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

if [[ ! -f "$zip_src" ]]; then
  echo "Missing $zip_src" >&2
  exit 1
fi

want="8f59cc41f9c1d4948fbbc305e45c789dc926f2bec9e87c689c56abdab57983c5"
got="$(sha256sum "$zip_src" | awk '{print $1}')"
if [[ "$got" != "$want" ]]; then
  echo "Wallet.zip SHA256 mismatch: $got" >&2
  exit 1
fi

mkdir -p "$stage/VDS-Wallet"
unzip -q -o "$zip_src" -d "$stage/unpack"
# Official zip has top-level wallet/
cp -a "$stage/unpack/wallet/." "$stage/VDS-Wallet/"
install -m 0644 "$root/VDS-Wallet/Start-VDS-Wallet.bat" "$stage/VDS-Wallet/Start-VDS-Wallet.bat"
install -m 0644 "$root/VDS-Wallet/Installieren.bat" "$stage/VDS-Wallet/Installieren.bat"
install -m 0644 "$root/VDS-Wallet/Lies-mich.txt" "$stage/VDS-Wallet/Lies-mich.txt"
mkdir -p "$stage/VDS-Wallet/data"
printf '%s\n' '# Portable datadir — wallet.dat and chain live here.' > "$stage/VDS-Wallet/data/README.txt"

mkdir -p "$root/dist"
rm -f "$out"
(cd "$stage" && zip -qr "$out" VDS-Wallet)
echo "Wrote $out"
ls -l "$out"
unzip -l "$out" | head -30
