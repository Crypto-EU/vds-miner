# vds-miner

OpenCL-**GPU**-Miner für **VDS (Vollar / V-Dimension)**. Algorithmus: **Equihash(96,5) + Scrypt-1024-1-1**. Läuft **nur auf der Grafikkarte** — ausgelegt für **AMD RX 6800 XT**, **RX 5700 XT** und HiveOS. Pool: **[666pool](https://www.666pool.com/)**.

Ohne OpenCL-GPU startet der Miner nicht.

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

## Code

Öffentliches GitHub-Repo: **https://github.com/Crypto-EU/vds-miner**

HiveOS-Download (Release, ohne Login, **glibc 2.17 / HiveOS**):

```
https://github.com/Crypto-EU/vds-miner/releases/download/v1.1.6/vds-miner-1.1.6.tar.gz
```

## Voraussetzungen

- Linux x86_64 (HiveOS, Ubuntu 20.04+, oder ähnliches)
- **AMD-GPU mit OpenCL** (kein CPU-Mining)
  - HiveOS: OpenCL ist in den AMD-Images enthalten
  - Desktop: `amdgpu-pro` OpenCL oder ROCm
- `g++` mit C++17, CMake ≥ 3.16, `opencl-headers`, `ocl-icd-opencl-dev`

RX 6800 XT = Navi 21 / `gfx1030`. RX 5700 XT = Navi 10 / `gfx1010`.

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

Wallet-Adresse einsetzen (beginnt mit `Vc`). **Anleitung:** [docs/vds-wallet.md](docs/vds-wallet.md).

**Wallet (empfangen und senden, Windows, Doppelklick):** [VDS-Wallet.zip](https://github.com/Crypto-EU/vds-miner/releases/download/v1.2.0/VDS-Wallet.zip) nach `C:\VDS-Wallet` entpacken, `Start-VDS-Wallet.bat` starten. Anleitung: [docs/vds-wallet.md](docs/vds-wallet.md). Die alte `core-qt.exe` von 2019 startet auf der aktuellen Chain nicht.

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
| `--autotune` | Pipelines und Workgroups messen (Standard **an**). Ergebnis in `vds-miner.tune` |
| `--no-autotune` | Autotune aus |
| `--autotune-force` | Cache ignorieren und neu messen |
| `--intensity` | nur ohne Autotune: Wagner-Pipelines je GPU, `1` oder `2` |
| `--api-port` | JSON-API für HiveOS, Standard `4068` |

Status nach ein paar Minuten auf [666pool](https://www.666pool.com/) prüfen. Das Log zeigt Hashrate in **MH/s** (1000 Sol/s = 1 MH/s). Shares erscheinen als `Share akzeptiert`. Bei der aktuellen Pool-Schwierigkeit kann das erste Share **30–90 Minuten** dauern — `shares A/R 0/0` heißt nicht, dass der Miner steht.

## HiveOS Flight Sheet

Vollständige Klick-Anleitung: **[docs/hiveos-flightsheet.md](docs/hiveos-flightsheet.md)**

Kurzwerte für **Flight Sheet → Miner = Custom**:

| Feld | Wert |
|---|---|
| Coin | `VDS` |
| Wallet | deine VDS-Adresse (`V…`) |
| Pool URL | `stratum+tcp://vds.666pool.com:9338` |
| Miner name | `vds-miner` |
| Installation URL | `https://github.com/Crypto-EU/vds-miner/releases/download/v1.1.6/vds-miner-1.1.6.tar.gz` |
| Wallet and worker template | `%WAL%.%WORKER_NAME%` |
| Pass | `x` |
| Extra config | leer = **Autotune** (beste Hashrate je Kartentyp). Optional: `-d 0,1` oder `--no-autotune --intensity 1` |

Paket bauen:

```bash
./scripts/build.sh
chmod +x hiveos/*.sh
./hiveos/package-hiveos.sh 1.1.6
```

HiveOS lädt das Archiv selbst. Nach einem Update **Installation URL** auf v1.1.6 stellen und neu laden:

```bash
/hive/miners/custom/custom-get https://github.com/Crypto-EU/vds-miner/releases/download/v1.1.6/vds-miner-1.1.6.tar.gz -f
```

Die mitgelieferte Binary ist gegen **glibc 2.17** gebaut (HiveOS 18.04+). Passt sie trotzdem nicht, kompiliert `h-run.sh` automatisch auf dem Rig.

HiveOS liest die Hashrate über `http://127.0.0.1:4068/`.

## Wie es funktioniert

VDS nutzt Equihash **n=96, k=5** (Blake2b `ZcashPoW`) und danach **scrypt(N=1024, r=1, p=1)** über den 281-Byte-Header.

Die Rechenarbeit läuft auf der GPU:

1. Stratum `mining.subscribe` / `authorize` / `notify`
2. OpenCL erzeugt die 131072 Blake2b-Hashes **auf der GPU**
3. OpenCL löst die Wagner-Kollisionen **auf der GPU**
4. Host prüft nur gefundene Solutions (scrypt gegen Pool-Target) und sendet `mining.submit`

Ab **v1.1.6** misst der Miner beim ersten Start **Autotune** (1 vs. 2 Pipelines, Workgroup 64/128/256) und speichert die schnellste Kombination in `vds-miner.tune`. Gleiche Karten (z. B. acht RX 5700 XT) werden nur einmal gemessen. Spätere Starts laden den Cache. `--autotune-force` misst neu.

## Lizenz

MIT. BLAKE2b-Referenz: CC0 / RFC 7693. Equihash-Logik nach Zcash/VDS (`v-dimension/vds-core`).
