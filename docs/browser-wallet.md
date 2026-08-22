# Browser-Wallet: empfangen und senden

Die Desktop-Datei `core-qt.exe` muss nicht funktionieren. Sie ist von 2019 und stoppt bei Block 193 076; die Chain ist viel weiter.

Diese Anleitung nutzt nur den **Browser** auf deinem PC.

Am Ende brauchst du:

1. eine Adresse, die mit **`Vc`** beginnt  
2. eine Backup-Datei mit dem **privaten Schlüssel (WIF)**  
3. diese `Vc…`-Adresse im HiveOS-Flight-Sheet  

Es gibt **keine** MetaMask-Wallet für VDS. Eine `0x…`-Adresse ist falsch.

---

## 0. Node.js

Auf dem **normalen PC** (Windows, nicht der HiveOS-Rig):

1. https://nodejs.org → **LTS**  
2. Terminal neu öffnen  
3. `node -v` und `npm -v` müssen eine Nummer zeigen  

---

## 1. Projekt

**ZIP:** https://github.com/Crypto-EU/vds-miner → Code → Download ZIP, entpacken.

**Git:**

```bash
git clone https://github.com/Crypto-EU/vds-miner.git
```

---

## 2. Starten

```bash
cd web-wallet
npm install
npm run dev
```

(`cd` anpassen, falls der Ordner anders heißt.)

Warten auf `Local: http://localhost:43187/` — Fenster **offen lassen**.

---

## 3. Wallet

Browser: **http://127.0.0.1:43187**

- **Neue Wallet erzeugen** oder WIF einfügen (derselbe Schlüssel wie fürs Mining)  
- **Empfangen:** Adresse kopieren, QR zeigen, Kontostand vom Explorer  
- **Senden:** Ziel-`Vc…` und Betrag. Geht erst, wenn Coins **on-chain** sind (Pool zahlt ab 2 VDS)  
- **Pool:** unbezahltes 666pool-Guthaben  
- **Schlüssel:** Backup herunterladen, USB, nicht in die Cloud  

---

## 4. HiveOS

1. Flight Sheet **Wallet:** nur die `Vc…`-Adresse  
2. Template `%WAL%.%WORKER_NAME%`  
3. Pool `stratum+tcp://vds.666pool.com:9338`, Pass `x`  

Miner-Log: keine Meldung `Invalid wallet address`.

---

## Kurzcheck

- [ ] `npm run dev`, Port **43187**  
- [ ] Adresse beginnt mit **`Vc`**  
- [ ] Backup / WIF offline  
- [ ] HiveOS-Wallet = genau diese Adresse  
- [ ] Log ohne `Invalid wallet address`
