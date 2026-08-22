# Browser-Wallet: Vc-Adresse erzeugen (Schritt für Schritt)

Die Desktop-Datei `core-qt.exe` muss nicht funktionieren. Diese Anleitung nutzt nur den **Browser**.

Es gibt **keine** MetaMask-Wallet für VDS. Eine `0x…`-Adresse ist falsch.

Am Ende brauchst du:

1. eine Adresse, die mit **`Vc`** beginnt  
2. eine Backup-Datei mit dem **privaten Schlüssel (WIF)**  
3. diese `Vc…`-Adresse im HiveOS-Flight-Sheet

---

## 0. Einmalig: Node.js installieren

Auf dem **normalen PC** (Windows, nicht der HiveOS-Rig):

1. https://nodejs.org öffnen  
2. die **LTS**-Version herunterladen und installieren (Häkchen „Add to PATH“ lassen)  
3. PowerShell oder Terminal **neu** öffnen  
4. prüfen:

```bash
node -v
npm -v
```

Beide Befehle müssen eine Versionsnummer zeigen. Wenn „nicht gefunden“: PC neu starten und nochmal versuchen.

---

## 1. Projekt holen

**Variante A — ZIP (einfach)**

1. Im Browser öffnen: https://github.com/Crypto-EU/vds-miner  
2. grüner Button **Code → Download ZIP**  
3. ZIP entpacken, z. B. nach `C:\Users\<DeinName>\Downloads\vds-miner-main\`

**Variante B — Git**

```bash
git clone https://github.com/Crypto-EU/vds-miner.git
```

---

## 2. Wallet-Seite starten

PowerShell / Terminal:

```bash
cd C:\Users\<DeinName>\Downloads\vds-miner-main\web-wallet
npm install
npm run dev
```

(`cd`-Pfad anpassen, falls der Ordner anders heißt.)

Warten, bis etwa steht:

```
Local: http://localhost:43187/
```

Das Fenster **offen lassen**. Schließen beendet die Wallet-Seite.

---

## 3. Im Browser öffnen

1. Chrome, Edge oder Firefox öffnen  
2. Adresszeile: **http://127.0.0.1:43187**  
3. Die Seite „VDS Browser-Wallet“ muss erscheinen  
4. Oben sollte stehen, dass Prefix `Vc` gültig ist (grüner Hinweis)

Leere Seite oder Fehler: Terminal-Ausgabe prüfen, `npm install` wiederholen, anderen Browser versuchen. Die Seite läuft **nur auf diesem PC**, nicht auf dem Rig und nicht ohne Schritt 2.

---

## 4. Adresse erzeugen

1. Button **Neue Vc-Adresse erzeugen**  
2. Unter **Deine Mining-Adresse** erscheint eine lange Zeile, die mit **`Vc`** beginnt  
3. **Adresse kopieren** klicken  
4. In einem Editor (Notepad) zur Kontrolle einfügen: erstes Zeichenpaar muss `Vc` sein, **kein** `0x`

Wenn die Adresse nicht mit `Vc` beginnt: nicht ins Flight Sheet eintragen, Seite neu laden, nochmal erzeugen.

---

## 5. Privaten Schlüssel sichern (Pflicht)

Ohne diesen Schritt sind später alle Coins weg.

1. **Backup herunterladen** — Datei `vds-wallet-backup.txt` landet meist in `Downloads`  
2. Datei auf einen **USB-Stick** kopieren, **nicht** in die Cloud, nicht an den Pool, nicht nach HiveOS  
3. Optional **Schlüssel anzeigen** und WIF auf Papier schreiben  
4. Die Datei enthält:
   - `Address:` → HiveOS  
   - `WIF:` → geheim, das ist der Besitznachweis

Dieselbe Adresse später wiederherstellen: auf der Startseite **Vorhandenen Schlüssel einfügen**, WIF einfügen, **Adresse wiederherstellen**. Es muss dieselbe `Vc…`-Adresse erscheinen.

---

## 6. In HiveOS eintragen

1. HiveOS-Web: **Workers → Flight Sheets** → dein VDS-Sheet  
2. Feld **Wallet:** nur die `Vc…`-Adresse (kein `.RIG…`, kein Leerzeichen)  
3. **Wallet and worker template:** `%WAL%.%WORKER_NAME%`  
4. **Pool:** `stratum+tcp://vds.666pool.com:9338`  
5. **Pass:** `x`  
6. Speichern und dem Worker zuweisen  

HiveOS hängt den Rig-Namen selbst an. Der Pool sieht z. B. `VcDeineAdresse.RIG_2_RX_5700_XT_Red_Devil_Asrock`.

Miner-Log: keine Meldung `Invalid wallet address`. Nach den ersten Shares [666pool](https://www.666pool.com/) prüfen.

Flight-Sheet-Felder: [hiveos-flightsheet.md](hiveos-flightsheet.md)

---

## 7. Was diese Wallet nicht kann

- Coins **ausgeben** oder den Kontostand anzeigen (kein Node)  
- MetaMask ersetzen  
- auf dem HiveOS-Rig laufen müssen — nur auf dem PC, einmalig zum Erzeugen der Adresse  

Zum späteren Ausgeben den **WIF** in eine volle VDS-Node-Wallet importieren (`importprivkey`), sobald eine aktuelle Node läuft.

---

## Kurzcheck

- [ ] Node.js LTS installiert  
- [ ] `npm run dev` läuft, Seite auf Port **43187**  
- [ ] Adresse beginnt mit **`Vc`**  
- [ ] Backup-Datei / WIF offline gesichert  
- [ ] HiveOS-Wallet-Feld = nur diese `Vc…`-Adresse  
- [ ] Log ohne `Invalid wallet address`
