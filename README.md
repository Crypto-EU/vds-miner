# vds-miner

OpenCL-GPU-Miner für **VDS (Vollar / V-Dimension)**. Algorithmus: **Equihash(96,5) + Scrypt-1024-1-1**. Gebaut für **AMD RX 6800 XT**, **RX 5700 XT** und HiveOS-Rigs, voreingestellt auf den Pool **[666pool](https://www.666pool.com/)**.

Pool-Stratum:

| | |
|---|---|
| Host | `vds.666pool.com` |
| Port | `9338` |
| SSL | `9339` (dieser Miner nutzt TCP 9338) |
| User | `VDS-Adresse.Workername` |
| Pass | `x` |
| Gebühr | 1 % PPLNS (Pool) |

Der Miner selbst hat **keine Dev-Fee**.

## Voraussetzungen

- Linux x86_64 (HiveOS, Ubuntu 20.04+, oder ähnliches)
- AMD-Treiber mit OpenCL:
  - HiveOS: OpenCL ist in den AMD-Images enthalten
  - Desktop: `amdgpu-pro` OpenCL oder ROCm
- `g++` mit C++17, CMake ≥ 3.16, `opencl-headers`, `ocl-icd-opencl-dev`

RX 6800 XT = Navi 21 / `gfx1030`. RX 5700 XT = Navi 10 / `gfx1010`. Beide laufen über denselben OpenCL-Pfad; der Miner erkennt die Karten und passt die Worker-Zahl an.

## Bauen

```bash
./scripts/build.sh
# oder
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Checks:

```bash
./build/vds-miner --self-test
./build/vds-miner --list-devices
./build/vds-miner --benchmark
```

## Mining auf 666pool

Wallet-Adresse einsetzen (beginnt typischerweise mit `V`):

```bash
./scripts/mine-666pool.sh VcDEINEADRESSE rig1
```

Oder direkt:

```bash
./build/vds-miner \
  -o stratum+tcp://vds.666pool.com:9338 \
  -u VcDEINEADRESSE.rig1 \
  -p x \
  -d 0,1
```

Optionen:

| Flag | Bedeutung |
|---|---|
| `-o` | Stratum-URL |
| `-u` | Wallet.Worker |
| `-p` | Passwort, Standard `x` |
| `-d` | OpenCL-Geräte, z.B. `0` oder `0,1,2` |
| `-t` | CPU-Wagner-Threads pro GPU (Standard 2) |
| `--intensity` | 1–24, mehr parallele Nonces |
| `--cpu-only` | Nur CPU (Fallback ohne OpenCL) |
| `--api-port` | JSON-API für HiveOS, Standard `4068` |

Status nach ein paar Minuten auf [666pool](https://www.666pool.com/) prüfen (Worker unter deiner Wallet). Shares erscheinen im Log als `Share akzeptiert`.

## HiveOS

1. Binary und Paket bauen:

```bash
./scripts/build.sh
chmod +x hiveos/*.sh
./hiveos/package-hiveos.sh 1.0.0
```

Es entsteht `dist/vds-miner-1.0.0.tar.gz`.

2. Archiv auf einen HTTP-Host legen (GitHub Releases, eigener Webspace). In HiveOS:

**Flight Sheet → Miner = Custom**

| Feld | Wert |
|---|---|
| Coin | VDS (oder Custom) |
| Wallet | deine VDS-Adresse |
| Pool URL | `stratum+tcp://vds.666pool.com:9338` |
| Miner name | `vds-miner` |
| Installation URL | `https://…/vds-miner-1.0.0.tar.gz` |
| Wallet and worker template | `%WAL%.%WORKER_NAME%` |
| Extra config | z.B. `-d 0,1 --intensity 12` |

Ohne URL kannst du auf dem Rig installieren:

```bash
/hive/miners/custom/custom-get https://…/vds-miner-1.0.0.tar.gz -f
```

HiveOS liest die Hashrate über `http://127.0.0.1:4068/`.

Wenn das vorgebaute Binary wegen einer neueren glibc nicht startet, liegt der Quellcode im Paket. `h-run.sh` versucht dann automatisch zu kompilieren (`g++` muss auf dem Rig verfügbar sein).

## Wie es funktioniert

VDS-Blöcke nutzen Zcash-Equihash mit **n=96, k=5** (Blake2b-Personalization `ZcashPoW`) und danach **scrypt(N=1024, r=1, p=1)** über den 281-Byte-Header (180 Prefix + 32 Nonce + CompactSize `0x44` + 68-Byte-Solution).

Ablauf:

1. Stratum `mining.subscribe` / `mining.authorize` / `mining.notify` (VDS-Header mit nVibPool, Sapling-, State- und UTXO-Root)
2. OpenCL erzeugt die 131072 Blake2b-Hashes auf der GPU
3. CPU löst Wagner-Runden und prüft die Solution
4. scrypt gegen das Pool-Target, bei Treffer `mining.submit`

Ohne OpenCL-GPU fällt der Miner auf reines CPU-Mining zurück.

## Hinweis zur Hashrate

Das ist ein offener Equihash-96,5-Miner, kein geschlossener GMiner/T-Rex-Kernel. Auf 6800 XT / 5700 XT ist die Hashrate nutzbar, liegt aber unter proprietären Minern. Intensity und `-t` an die CPU des Rigs anpassen (Wagner läuft auf der CPU, Blake2b auf der GPU).

## Lizenz

MIT. BLAKE2b-Referenz: CC0 / RFC 7693. Equihash-Logik nach Zcash/VDS (`v-dimension/vds-core`).
