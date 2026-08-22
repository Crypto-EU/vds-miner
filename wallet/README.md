# VDS-Wallet (Windows)

Hier liegen die heruntergeladenen Wallet-Dateien. Quelle:

https://github.com/V-Dimension-VDS/vds-core/releases/download/VDS/Wallet.zip

SHA256 von `Wallet.zip`:

```
8f59cc41f9c1d4948fbbc305e45c789dc926f2bec9e87c689c56abdab57983c5
```

Das ist die Windows-Oberfläche **core-qt.exe** (Vds 0.9.9) plus mitgelieferte Java-Runtime. Damit erzeugst du eine Adresse `Vc…` für 666pool.

Ausführliche Schritte: [docs/vds-wallet.md](../docs/vds-wallet.md)

---

## Start (Windows-PC, nicht HiveOS-Rig)

1. Diesen Ordner auf den Windows-Rechner kopieren (oder `Wallet.zip` entpacken).
2. `windows\core-qt.exe` doppelklicken.
3. Beim ersten Start wird automatisch eine neue Wallet angelegt.
4. **Settings → Encrypt Wallet** — eigenes Passwort setzen.
5. **File → Backup Wallet** — `wallet.dat` auf USB sichern.
6. Reiter **Receive** / Konsole `getnewaddress` — Adresse muss mit **`Vc`** beginnen.
7. Diese `Vc…`-Adresse ins HiveOS-Flight-Sheet-Feld **Wallet**.

Datenordner der Wallet:

```
C:\Users\<DeinName>\AppData\Roaming\Vds\wallet.dat
```

---

## Hinweis

Die Datei stammt von 2019. Vds-Core kann Nodes nach einer Frist stoppen (Deprecation). Wenn `core-qt.exe` nicht startet oder sich sofort beendet, Wallet trotzdem nicht von fremden APK-Seiten laden — dann Weg B in `docs/vds-wallet.md` (aus Quellcode bauen).

Keine `0x…`-ETH-Adresse verwenden.
