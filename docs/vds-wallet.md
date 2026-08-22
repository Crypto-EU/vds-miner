# VDS-Wallet anlegen (Adresse `Vc…` für 666pool / HiveOS)

666pool zahlt VDS (Vollar) nur auf eine **Transparenz-Adresse der VDS-Chain**. Sie beginnt mit **`Vc`**.  
MetaMask, Trust Wallet und jede `0x…`-Adresse sind **Ethereum** — dafür gibt es **keine** Auszahlung.

## Was du nutzt

**Windows, Doppelklick** — kein Node.js:

1. ZIP: https://github.com/Crypto-EU/vds-miner/releases/download/v1.2.0/VDS-Wallet.zip  
2. Nach `C:\VDS-Wallet` entpacken  
3. `Start-VDS-Wallet.bat` starten  

Der Browser öffnet **http://127.0.0.1:43187**. Das schwarze Fenster offen lassen.

Die Wallet kann:

- eine `Vc`-Adresse erzeugen oder per WIF öffnen  
- **Empfangen** (Adresse + QR, HiveOS-Feld Wallet)  
- **Kontostand** vom Explorer [vdscool.com](https://vdscool.com/)  
- **Senden** an eine andere `Vc…`-Adresse  
- **666pool**-Guthaben anzeigen (Auszahlung ab **2 VDS**)

Schritt für Schritt: **[VDS-Wallet/Lies-mich.txt](../VDS-Wallet/Lies-mich.txt)**

## Warum `core-qt.exe` / die alte `C:\VDS-Wallet` nicht geht

Die originale Wallet (Vds 0.9.9, 2019) schaltet den Node nach Blockhöhe **193 076** ab. Die Chain ist bei über **3,6 Millionen** Blöcken. SmartScreen und die mitgelieferte Java-Runtime von 2019 kommen dazu. **Nicht weiter versuchen** — die neue `vds-wallet.exe` ersetzt das.

## HiveOS

| Feld | Wert |
|---|---|
| Wallet | nur `Vc…` (kein `.RIG…`, kein `0x`) |
| Wallet and worker template | `%WAL%.%WORKER_NAME%` |
| Pool | `stratum+tcp://vds.666pool.com:9338` |
| Pass | `x` |

## Sicherheit

- WIF/`wallet.dat`/Passwort niemandem schicken.  
- Dieselbe `Vc…`-Adresse darfst du auf mehreren Rigs nutzen.  
- Eine Adresse, die dir jemand „fertig“ schickt, gehört **ihm**.
