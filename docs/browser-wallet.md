# Browser-Wallet: empfangen und senden

Die Desktop-Datei `core-qt.exe` von 2019 startet nicht. Sie stoppt bei Block 193 076; die Chain ist viel weiter.

**Normale Nutzung (Windows):** ZIP herunterladen und doppelklicken — **kein Node.js**.

https://github.com/Crypto-EU/vds-miner/releases/download/v1.2.0/VDS-Wallet.zip

Nach `C:\VDS-Wallet` entpacken, `Start-VDS-Wallet.bat` starten. Details: [Lies-mich.txt](../VDS-Wallet/Lies-mich.txt).

Am Ende brauchst du:

1. eine Adresse, die mit **`Vc`** beginnt  
2. eine Backup-Datei mit dem **privaten Schlüssel (WIF)**  
3. diese `Vc…`-Adresse im HiveOS-Flight-Sheet  

Es gibt **keine** MetaMask-Wallet für VDS. Eine `0x…`-Adresse ist falsch.

---

## Entwickler: aus dem Quellcode

Nur wenn du die Oberfläche ändern willst.

### 0. Node.js

1. https://nodejs.org → **LTS**  
2. `node -v` und `npm -v` müssen eine Nummer zeigen  

### 1. Projekt

```bash
git clone https://github.com/Crypto-EU/vds-miner.git
```

### 2. Dev-Server

```bash
cd web-wallet
npm install
npm run dev
```

Browser: **http://127.0.0.1:43187**

Portable Windows-Datei bauen: `./scripts/build-wallet.sh`

---

## Wallet nutzen

- **Neue Wallet erzeugen** oder WIF einfügen (derselbe Schlüssel wie fürs Mining)  
- **Empfangen:** Adresse kopieren, QR zeigen, Kontostand vom Explorer  
- **Senden:** Ziel-`Vc…` und Betrag. Geht erst, wenn Coins **on-chain** sind (Pool zahlt ab 2 VDS)  
- **Pool:** unbezahltes 666pool-Guthaben  
- **Schlüssel:** Backup herunterladen, USB, nicht in die Cloud  

---

## HiveOS

1. Flight Sheet **Wallet:** nur die `Vc…`-Adresse  
2. Template `%WAL%.%WORKER_NAME%`  
3. Pool `stratum+tcp://vds.666pool.com:9338`, Pass `x`  

Miner-Log: keine Meldung `Invalid wallet address`.

---

## Kurzcheck

- [ ] `Start-VDS-Wallet.bat` bzw. Seite auf Port **43187**  
- [ ] Adresse beginnt mit **`Vc`**  
- [ ] Backup / WIF offline  
- [ ] HiveOS-Wallet = genau diese Adresse  
- [ ] Log ohne `Invalid wallet address`
