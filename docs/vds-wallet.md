# VDS-Wallet anlegen (Adresse `Vc…` für 666pool / HiveOS)

666pool zahlt VDS (Vollar) nur auf eine **Transparenz-Adresse der VDS-Chain**. Sie beginnt mit **`Vc`**.  
MetaMask, Trust Wallet und jede `0x…`-Adresse sind **Ethereum** — dafür gibt es **keine** Auszahlung.

## Was funktioniert

Die **Browser-Wallet** in diesem Repo kann:

- eine `Vc`-Adresse erzeugen oder per WIF öffnen  
- **Empfangen** (Adresse + QR, HiveOS-Feld Wallet)  
- **Kontostand** vom Explorer [vdscool.com](https://vdscool.com/)  
- **Senden** an eine andere `Vc…`-Adresse  
- **666pool**-Guthaben anzeigen (Auszahlung ab **2 VDS**)

Schritt für Schritt: **[docs/browser-wallet.md](browser-wallet.md)** und **[web-wallet/README.md](../web-wallet/README.md)**

```bash
cd web-wallet
npm install
npm run dev
```

Browser: http://127.0.0.1:43187

## Warum `core-qt.exe` / `C:\VDS-Wallet` nicht geht

Die originale Wallet (Vds 0.9.9, 2019) schaltet den Node nach Blockhöhe **193 076** ab. Die Chain ist bei über **3,6 Millionen** Blöcken. SmartScreen und die mitgelieferte Java-Runtime von 2019 kommen dazu. **Nicht weiter versuchen** — die Browser-Wallet ersetzt das.

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
