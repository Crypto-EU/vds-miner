# VDS-Wallet: empfangen und senden

Die alte Windows-Datei `core-qt.exe` (2019) startet nicht sinnvoll: sie beendet sich nach Blockhöhe **193 076**. Die Chain steht bei über **3,6 Millionen** Blöcken.

Diese Wallet läuft im **Browser auf deinem PC**, spricht den lebenden Explorer [vdscool.com](https://vdscool.com/) und 666pool an, und kann VDS **empfangen und senden**.

Am Ende brauchst du:

1. eine Adresse, die mit **`Vc`** beginnt  
2. eine Backup-Datei mit dem **privaten Schlüssel (WIF)**  
3. dieselbe `Vc…`-Adresse im HiveOS-Flight-Sheet  

Es gibt **keine** MetaMask-Wallet für diese Chain. Eine `0x…`-Adresse ist falsch.

---

## 0. Einmalig: Node.js

Auf dem **normalen PC** (nicht der HiveOS-Rig):

1. https://nodejs.org → **LTS** installieren  
2. Terminal neu öffnen  
3. `node -v` und `npm -v` müssen eine Versionsnummer zeigen  

---

## 1. Projekt holen

ZIP: https://github.com/Crypto-EU/vds-miner → **Code → Download ZIP**, entpacken.

Oder:

```bash
git clone https://github.com/Crypto-EU/vds-miner.git
```

---

## 2. Wallet starten

```bash
cd vds-miner/web-wallet
npm install
npm run dev
```

Warten auf:

```
Local: http://localhost:43187/
```

Das Fenster **offen lassen**.

---

## 3. Im Browser

http://127.0.0.1:43187

- **Neue Wallet erzeugen**, oder **Vorhandenen Schlüssel einfügen** (dein WIF)  
- Reiter **Empfangen**: Adresse + QR, Kontostand on-chain  
- Reiter **Senden**: Vc-Adresse und Betrag  
- Reiter **Pool**: unbezahltes 666pool-Guthaben (Auszahlung ab **2 VDS**)  
- Reiter **Schlüssel**: WIF sichern  

---

## 4. HiveOS

Flight Sheet **Wallet:** nur die `Vc…`-Adresse.

Pool: `stratum+tcp://vds.666pool.com:9338`  
Template: `%WAL%.%WORKER_NAME%`  
Pass: `x`

---

## Senden

Senden braucht **on-chain Guthaben**. Der Pool schickt erst aus, wenn unbezahltes Guthaben **≥ 2 VDS** ist. Davor steht der Explorer auf 0 — das ist normal.

Empfänger muss mit `Vc` beginnen. Gebühr: 0,0001 VDS.

Explorer: https://vdscool.com/

---

## Kurzcheck

- [ ] Seite auf Port **43187**  
- [ ] Adresse beginnt mit **`Vc`**  
- [ ] Backup / WIF offline  
- [ ] HiveOS-Wallet-Feld = genau diese Adresse  
- [ ] Miner-Log ohne `Invalid wallet address`
