# VDS-Wallet: empfangen und senden

Die alte Windows-Datei `core-qt.exe` (2019) startet nicht sinnvoll: sie beendet sich nach Blockhöhe **193 076**. Die Chain steht bei über **3,6 Millionen** Blöcken.

**Zum Benutzen brauchst du kein Node.js.** Lade die Windows-Datei:

https://github.com/Crypto-EU/vds-miner/releases/download/v1.2.0/VDS-Wallet.zip

Nach `C:\VDS-Wallet` entpacken, **`Start-VDS-Wallet.bat`** doppelklicken. Der Browser öffnet http://127.0.0.1:43187.

Dieser Ordner `web-wallet/` ist nur der Quellcode der Oberfläche.

Am Ende brauchst du:

1. eine Adresse, die mit **`Vc`** beginnt  
2. eine Backup-Datei mit dem **privaten Schlüssel (WIF)**  
3. dieselbe `Vc…`-Adresse im HiveOS-Flight-Sheet  

Es gibt **keine** MetaMask-Wallet für diese Chain. Eine `0x…`-Adresse ist falsch.

---

## Entwickler

```bash
cd web-wallet
npm install
npm test
npm run dev
```

Portable `.exe` + ZIP:

```bash
./scripts/build-wallet.sh
```

---

## HiveOS

Flight Sheet **Wallet:** nur die `Vc…`-Adresse.

Pool: `stratum+tcp://vds.666pool.com:9338`  
Template: `%WAL%.%WORKER_NAME%`  
Pass: `x`

Senden braucht **on-chain Guthaben**. Der Pool schickt erst aus, wenn unbezahltes Guthaben **≥ 2 VDS** ist.

Explorer: https://vdscool.com/
