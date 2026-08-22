# VDS-Wallet (Windows) — Extraordner

Die originale Wallet gehört auf den **normalen PC**, in einen eigenen Ordner:

**`C:\VDS-Wallet`**

Nicht in den Miner-Ordner, nicht auf den HiveOS-Rig.

## Schnell

1. ZIP herunterladen: https://github.com/Crypto-EU/vds-miner/releases/download/v1.1.2/VDS-Wallet.zip
2. Nach `C:\VDS-Wallet` entpacken
3. `Start-VDS-Wallet.bat` starten

Im Repo: Ordner [VDS-Wallet/](../VDS-Wallet/) — `Installieren.bat` kopiert nach `C:\VDS-Wallet`.

Original-Archiv (Quelle 2019): [Wallet.zip](Wallet.zip)

https://github.com/V-Dimension-VDS/vds-core/releases/download/VDS/Wallet.zip

SHA256 von `Wallet.zip`:

```
8f59cc41f9c1d4948fbbc305e45c789dc926f2bec9e87c689c56abdab57983c5
```

Das ist **core-qt.exe** (Vds 0.9.9) plus mitgelieferte Java-Runtime. Damit erzeugst du eine Adresse `Vc…` für 666pool.

Ausführliche Schritte: [docs/vds-wallet.md](../docs/vds-wallet.md)

Daten:

```
C:\VDS-Wallet\data\wallet.dat
```

---

## Hinweis

Die Datei stammt von 2019. Windows-SmartScreen warnt deshalb. Vds-Core kann Nodes nach einer Frist stoppen. Wenn `core-qt.exe` nicht startet: [docs/browser-wallet.md](../docs/browser-wallet.md). Keine fremden APKs.

Keine `0x…`-ETH-Adresse verwenden.
