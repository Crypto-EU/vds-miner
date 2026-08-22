# VDS-Wallet anlegen (Adresse `Vc…` für 666pool / HiveOS)

666pool zahlt VDS (Vollar) nur auf eine **Transparenz-Adresse der VDS-Chain**. Sie beginnt mit **`Vc`**.  
MetaMask, Trust Wallet und jede `0x…`-Adresse sind **Ethereum** — dafür gibt es **keine** Auszahlung.

Du brauchst die Adresse **bevor** du im Flight Sheet minest. Die Blockchain muss dafür **nicht** fertig synchron sein; zum **Ausgeben** der Coins später schon.

---

## Was du am Ende haben musst

| | |
|---|---|
| Adresse | eine Zeile, beginnt mit `Vc`, z. B. `Vc1a2B3c…` (Länge typisch ~35 Zeichen) |
| Backup | Datei `wallet.dat` **und** das Wallet-Passwort |
| HiveOS | genau diese `Vc…`-Adresse ins Feld **Wallet** (ohne Worker-Namen) |

**Nicht** verwenden:

- VID / „Create VID Address“ (das ist eine Identitäts-Adresse, kostet Vollar, nicht die Pool-Wallet)
- `zs…` / Shielded-Adressen
- `0x…`
- zufällige APKs von Download-Portalen

---

## Weg A — Grafische VDS-Wallet (Windows, empfohlen)

Die Windows-Dateien liegen in diesem Repo, Ordner **[wallet/](../wallet/)**:

- Archiv: [wallet/Wallet.zip](../wallet/Wallet.zip)
- Entpackt: [wallet/windows/core-qt.exe](../wallet/windows/core-qt.exe)

Quelle des Downloads: [V-Dimension-VDS/vds-core Release „VDS“](https://github.com/V-Dimension-VDS/vds-core/releases/tag/VDS) (Vds 0.9.9, 2019).

### 1. Wallet starten

1. Ordner `wallet/` auf den **Windows-PC** kopieren (nicht auf den HiveOS-Rig).
2. `wallet\windows\core-qt.exe` starten.
3. Beim ersten Start legt die Software automatisch eine neue Wallet an.

Datenordner (Windows):

```
C:\Users\<DeinName>\AppData\Roaming\Vds\
```

Darin liegt später `wallet.dat`.

### 2. Wallet verschlüsseln

1. Menü **Settings → Encrypt Wallet** (oder chinesisch **设置 → 加密钱包**).
2. Ein **langes eigenes Passwort** setzen (nicht das Pool-Passwort `x`).
3. Passwort auf Papier notieren. Ohne Passwort ist `wallet.dat` nach dem Verschlüsseln wertlos.

Die Software startet danach oft neu.

### 3. Backup

1. Menü **File → Backup Wallet** (**文件 → 备份钱包**).
2. Die Kopie von `wallet.dat` auf einen USB-Stick legen, **nicht** in die Cloud, nicht an den Pool, nicht in HiveOS hochladen.
3. Zusätzlich den Ordner `%APPDATA%\Vds` sichern, sobald du Coins hast.

Ohne diese Datei sind geminte Coins **weg**.

---

## Weg C — Browser-Wallet (wenn core-qt nicht startet)

Vollständige Klick-Anleitung: **[docs/browser-wallet.md](browser-wallet.md)**

Kurz:

```bash
cd web-wallet
npm install
npm run dev
```

Browser: http://127.0.0.1:43187 → **Neue Vc-Adresse erzeugen** → Adresse ins HiveOS-Feld Wallet → **Backup herunterladen**.

### 4. Empfangsadresse holen (`Vc…`)

1. Reiter **Receive** / **收款**.
2. **Request payment** / neue Adresse anzeigen.
3. Die Adresse muss mit **`Vc`** beginnen. Kopieren.

Falls die Oberfläche keine Adresse zeigt:

1. **Help → Debug window → Console** (**帮助 → 调试窗口 → 控制台**)
2. Eingeben:

```
getnewaddress
```

3. Die ausgegebene `Vc…`-Zeile kopieren.

### 5. In HiveOS eintragen

Flight Sheet:

| Feld | Wert |
|---|---|
| Wallet | nur `Vc…` (kein `.RIG…`, kein `0x`) |
| Wallet and worker template | `%WAL%.%WORKER_NAME%` |
| Pool | `stratum+tcp://vds.666pool.com:9338` |
| Pass | `x` |

HiveOS hängt den Rig-Namen automatisch an. Der Pool sieht dann z. B. `VcDeineAdresse.RIG_2_RX_5700_XT_Red_Devil_Asrock`.

Flight Sheet speichern, dem Worker zuweisen, Miner-Log prüfen: keine Meldung `Invalid wallet address`.

---

## Weg B — Linux-Kommandozeile (nachvollziehbar, Quellcode)

Nur nötig, wenn du keinen vertrauenswürdigen Windows-Installer hast. Bauen dauert und braucht Abhängigkeiten (wie ein Zcash-Node).

```bash
sudo apt-get update
sudo apt-get install -y git build-essential pkg-config autoconf automake libtool \
  bsdmainutils curl wget python3

git clone https://github.com/v-dimension/vds-core.git
cd vds-core
./vcutil/build.sh
```

Danach (Binaries liegen unter `src/`):

```bash
./src/vdsd -daemon
sleep 5
./src/vds-cli getwalletinfo
./src/vds-cli getnewaddress
```

Die letzte Zeile ist deine `Vc…`-Adresse.

Wallet-Datei:

```
~/.vds/wallet.dat
```

Verschlüsseln:

```bash
./src/vds-cli encryptwallet 'DEIN-LANGES-PASSWORT'
```

Backup: `~/.vds/wallet.dat` kopieren, nachdem der Daemon steht (`./src/vds-cli stop`).

Optional GUI nach dem Build: `./src/vds-qt`.

---

## Kontrolle

Eine gültige Mining-Adresse:

- beginnt mit **`Vc`**
- enthält **kein** `0x`
- kommt aus **deiner** `wallet.dat`

Auf [666pool](https://www.666pool.com/) nach den ersten Shares den Worker suchen. Erscheint `Invalid wallet address`, ist es noch die ETH-Adresse oder ein Tippfehler.

VID brauchst du fürs Mining **nicht**.

---

## Sicherheit (kurz)

- Seed/`wallet.dat`/Passwort niemandem schicken — auch keinem „Support“.
- Dieselbe `Vc…`-Adresse darfst du auf mehreren Rigs nutzen (unterscheiden sich nur durch den Worker-Namen).
- Eine Adresse, die dir jemand „fertig“ schickt, gehört **ihm**, nicht dir.
