#!/usr/bin/env bash
# Build the portable VDS wallet (Windows .exe + zip, Linux binary for this machine).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
web="$root/web-wallet"
embed="$root/cmd/vds-wallet/web"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

cd "$web"
if [[ ! -d node_modules ]]; then
  npm install
fi
npm test
npm run build

rm -rf "$embed"
mkdir -p "$embed"
cp -a "$web/dist/." "$embed/"
# Keep a tiny file so `go:embed all:web` never sees an empty dir.
if [[ ! -f "$embed/index.html" ]]; then
  echo "vite build produced no index.html" >&2
  exit 1
fi

cd "$root/cmd/vds-wallet"
go test ./...

mkdir -p "$root/VDS-Wallet" "$root/dist" "$root/bin"
CGO_ENABLED=0 GOOS=windows GOARCH=amd64 go build -trimpath -ldflags="-s -w" -o "$root/VDS-Wallet/vds-wallet.exe"
CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -trimpath -ldflags="-s -w" -o "$root/bin/vds-wallet"

mkdir -p "$stage/VDS-Wallet"
cp -a "$root/VDS-Wallet/vds-wallet.exe" "$stage/VDS-Wallet/"
cp -a "$root/VDS-Wallet/Start-VDS-Wallet.bat" "$stage/VDS-Wallet/"
cp -a "$root/VDS-Wallet/Installieren.bat" "$stage/VDS-Wallet/"
cp -a "$root/VDS-Wallet/Lies-mich.txt" "$stage/VDS-Wallet/"
cp -a "$root/VDS-Wallet/README.md" "$stage/VDS-Wallet/"

rm -f "$root/dist/VDS-Wallet.zip"
(cd "$stage" && zip -qr "$root/dist/VDS-Wallet.zip" VDS-Wallet)

echo
echo "Windows: $root/VDS-Wallet/vds-wallet.exe"
echo "Linux:   $root/bin/vds-wallet"
echo "ZIP:     $root/dist/VDS-Wallet.zip"
ls -lh "$root/VDS-Wallet/vds-wallet.exe" "$root/bin/vds-wallet" "$root/dist/VDS-Wallet.zip"
unzip -l "$root/dist/VDS-Wallet.zip"
