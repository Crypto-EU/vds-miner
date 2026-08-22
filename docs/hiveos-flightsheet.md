# HiveOS Flight Sheet für vds-miner (VDS / 666pool)

Schritt-für-Schritt-Anleitung: Custom-Miner auf dem Rig einrichten und ein Flight Sheet anlegen.

Voraussetzungen:

- HiveOS mit **AMD-Image** (nicht Nvidia). OpenCL muss geladen sein.
- GPU: z. B. **RX 6800 XT** und/oder **RX 5700 XT**
- Echte **VDS-Wallet** (Adresse beginnt mit `Vc`) — Anlegen: **[docs/vds-wallet.md](vds-wallet.md)**
- Öffentliche **HTTPS-URL** zur Datei `vds-miner-1.1.6.tar.gz` (HiveOS lädt das Paket selbst herunter)

Ohne öffentliche URL: Paket per SCP auf den Rig kopieren — siehe Abschnitt 6.

---

## 1. Werte zum Kopieren

| HiveOS-Feld | Wert |
|---|---|
| Coin | `VDS` (falls nicht in der Liste: Coin selbst anlegen, Name *Vollar / VDS*) |
| Wallet | deine VDS-Adresse, z. B. `Vc…` |
| Pool URL | `stratum+tcp://vds.666pool.com:9338` |
| Miner | **Custom** |
| Miner name | `vds-miner` |
| Installation URL | `https://github.com/Crypto-EU/vds-miner/releases/download/v1.1.6/vds-miner-1.1.6.tar.gz` |
| Hash algorithm | leer lassen oder `equihash96_5` |
| Wallet and worker template | `%WAL%.%WORKER_NAME%` |
| Pass | `x` |
| Extra config arguments | leer = **Autotune** (beste Hashrate). Optional: `-d 0` / `-d 0,1`, `--no-autotune --intensity 1`, `--autotune-force` |
| API port | `4068` (steht bereits in `h-config.sh`, nicht extra eintragen) |

Der Miner-Name **muss** genau `vds-miner` heißen. HiveOS entpackt nach `/hive/miners/custom/vds-miner/`.

---

## 2. Coin anlegen (wenn VDS fehlt)

1. HiveOS-Web: **Workers → Flight Sheets**
2. **Add Flight Sheet**
3. Bei **Coin** auf das Plus / *Create coin* (je nach HiveOS-Version)
4. Ausfüllen:
   - **Name:** `VDS`
   - **Full name:** `Vollar`
   - **Algorithm:** `equihash` oder frei `equihash96_5`
5. Speichern und diese Coin im Flight Sheet auswählen

---

## 3. Flight Sheet anlegen

1. **Workers → Flight Sheets → Add Flight Sheet**
2. **Coin:** `VDS`
3. **Wallet:** deine VDS-Adresse (ohne Worker-Namen — den hängt HiveOS über das Template an)
4. **Pool:**
   - URL: `stratum+tcp://vds.666pool.com:9338`
   - SSL-Port `9339` **nicht** verwenden; dieser Miner spricht TCP auf **9338**
5. **Miner:** in der Liste **Custom** wählen
6. **Setup Miner Config** öffnen und eintragen:

```
Installation URL:     https://github.com/Crypto-EU/vds-miner/releases/download/v1.1.6/vds-miner-1.1.6.tar.gz
Miner name:           vds-miner
Hash algorithm:       (leer)
Wallet and worker template:  %WAL%.%WORKER_NAME%
Pass:                 x
Extra config arguments:      (leer = Autotune; oder -d 0,1 / --autotune-force)
```

7. Flight Sheet speichern, z. B. Name: `VDS 666pool vds-miner`

HiveOS setzt `%WAL%` auf die Wallet und `%WORKER_NAME%` auf den Rig-Namen. Der Pool sieht dann `VcDeineAdresse.rig1`.

---

## 4. Flight Sheet auf den Worker legen

1. **Workers** → deinen Rig anklicken
2. Oben **Flight Sheet** wählen → das neue Sheet `VDS 666pool vds-miner`
3. Warten, bis HiveOS das Archiv lädt und `vds-miner` startet
4. **Miner** / **Logs** öffnen. Erwartete Zeilen:
   - `Subscribe OK`
   - `Autotune: misst …` beim **ersten** Start (ca. 30–90 s für den ersten Kartentyp, danach Cache `vds-miner.tune`)
   - `GPU 0: Autotune-Wahl  pipes=… gen=… wg=…`
   - `GPU 0 (…): Equihash auf der GPU, N Pipeline(s)`
   - `GPU 0  0.160 MH/s  (…)` — Hashrate **pro Karte** (Summe der Pipelines), nicht pro Worker
   - später `Share akzeptiert` — bei der Pool-Schwierigkeit oft erst nach **30–90 Minuten**. `A/R 0/0` bedeutet nur: noch kein Share, der Miner läuft trotzdem.

Dashboard: [666pool](https://www.666pool.com/) — Worker und Hashrate erscheinen nach den ersten Shares.

---

## 5. Installation URL — woher kommt das `.tar.gz`?

HiveOS braucht eine **direkt herunterladbare** Datei, kein Git-Clone.

### Variante A — GitHub Release (empfohlen)

Direkt-Download (öffentlich, ohne Login):

```
https://github.com/Crypto-EU/vds-miner/releases/download/v1.1.6/vds-miner-1.1.6.tar.gz
```

Repo: https://github.com/Crypto-EU/vds-miner

Raw aus `main` (Fallback):

```
https://raw.githubusercontent.com/Crypto-EU/vds-miner/main/dist/vds-miner-1.1.6.tar.gz
```

Diese URL ins Feld **Installation URL** kopieren.

### Variante B — eigenes HTTP

`dist/vds-miner-1.1.6.tar.gz` auf beliebigen HTTPS-Host legen (Hetzner, S3, eigener Webserver). Die URL muss ohne Login funktionieren.

### Paket selbst bauen

Auf einem Linux-x86_64-Rechner mit OpenCL-Headers:

```bash
git clone https://DEIN-REPO/vds-miner.git
cd vds-miner
./scripts/build.sh
chmod +x hiveos/*.sh
./hiveos/package-hiveos.sh 1.1.6
# → dist/vds-miner-1.1.6.tar.gz
```

---

## 6. Ohne öffentliche URL (direkt auf den Rig)

Im HiveOS-Terminal des Workers (SSH oder Hive Shell):

```bash
# Archiv nach /tmp kopieren (scp vom PC, oder wget von irgendwo)
mkdir -p /hive/miners/custom
cd /tmp
tar -tzf vds-miner-1.1.6.tar.gz   # muss Ordner vds-miner/ zeigen
tar -C /hive/miners/custom -xzf vds-miner-1.1.6.tar.gz
ls -l /hive/miners/custom/vds-miner/vds-miner
chmod +x /hive/miners/custom/vds-miner/vds-miner
chmod +x /hive/miners/custom/vds-miner/h-*.sh
```

Danach trotzdem ein Flight Sheet mit Miner name `vds-miner` anlegen. Installation URL kannst du auf dieselbe Datei zeigen lassen, oder HiveOS mit bereits entpacktem Miner starten:

```bash
/hive/miners/custom/custom-get https://DEINE-URL/vds-miner-1.1.6.tar.gz -f
miner start
```

---

## 7. Extra config (optional)

| Extra config | Wirkung |
|---|---|
| *(leer)* | alle AMD-GPUs, **Autotune** (beste Hashrate, Cache `vds-miner.tune`) |
| `-d 0` | nur GPU 0 (trotzdem Autotune) |
| `-d 0,1` | GPU 0 und 1 |
| `--autotune-force` | Cache ignorieren, Pipelines/Workgroups neu messen |
| `--no-autotune` | 2 Pipelines, Standard-Workgroups |
| `--no-autotune --intensity 1` | eine Pipeline je GPU |

Der erste Start nach einem Update dauert etwas länger (Messung). Danach startet der Miner sofort aus dem Cache. `--cpu-only` und `-t` sind **abgeschaltet**. Der Miner läuft nur auf der GPU.

---

## 8. Kontrolle und Fehler

| Symptom | Check |
|---|---|
| `Compile failed` / Binary passt nicht zu glibc | Paket **v1.1.6** (glibc 2.17, kein Kompilieren auf dem Rig). Alte 1.1.2/1.1.3-Pakete hatten eine zu neue Binary und ein kaputtes `compile.sh`. |
| Rig startet neu, sobald **LA > 4** | HiveOS-Watchdog. Miner **v1.1.6** senkt LA (Scrypt nicht mehr auf jedem GPU-Thread). Trotzdem in HiveOS: Worker → **Watchdog** → Load Average auf **30** oder aus. SSH: in `/hive/bin/wd` steht `WD_DEF_LA=$(( CORES + 20 ))` — nicht auf 4 lassen. |
| `shares A/R 0/0`, GPUs rechnen | Normal. Pool-Target braucht oft **30–90 min** bis zum ersten Share. Log: `Bester PoW` und `im Mittel ~…/Share`. |
| HiveOS zeigt 0.00 MH/s / 0 H/s | Paket **v1.1.6** (Hashrate in MH/s). API-Port **4068** nicht von einem anderen Miner belegen. |
| `Invalid wallet address` | Adresse beginnt nicht mit `V`, oder Tippfehler. Dummy-Wallet ablehnen der Pool. |
| Custom miner wird nicht entpackt | Miner name nicht `vds-miner`, oder tar.gz hat keinen Top-Level-Ordner `vds-miner/`. |
| Compile on rig | Wenn die Binary nicht passt, braucht der Rig `g++` und OpenCL-Dev. Besser: passendes Linux-Binary ins Paket legen. |

Logs:

```bash
tail -f /var/log/miner/vds-miner/vds-miner.log
curl -s http://127.0.0.1:4068/
```

Pool-Gebühr: **1 % PPLNS** bei 666pool. Der Miner selbst hat **keine Dev-Fee**.

---

## 9. Flight Sheet — Kurzcheckliste

- [ ] Coin `VDS`, Wallet `V…`
- [ ] Pool `stratum+tcp://vds.666pool.com:9338`
- [ ] Miner **Custom**, Name `vds-miner`
- [ ] Installation URL zeigt auf `vds-miner-1.1.6.tar.gz`
- [ ] Template `%WAL%.%WORKER_NAME%`, Pass `x`
- [ ] Sheet dem Worker zugewiesen
- [ ] Log: GPU erkannt, Shares akzeptiert
- [ ] [666pool](https://www.666pool.com/) zeigt den Worker
