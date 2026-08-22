# VDS Browser-Wallet

Es gibt **keine** MetaMask-/Chrome-Wallet für die echte VDS-Chain (Vollar). `0x…`-Adressen lehnt 666pool ab.

Diese Seite erzeugt im Browser eine Adresse `Vc…` plus privaten Schlüssel (WIF). Die Schlüssel bleiben lokal.

Vollständige Anleitung: **[docs/browser-wallet.md](../docs/browser-wallet.md)**

```bash
cd web-wallet
npm install
npm test
npm run dev
```

Öffnen: http://127.0.0.1:43187

1. **Neue Vc-Adresse erzeugen**
2. Adresse kopieren → HiveOS Flight Sheet, Feld Wallet
3. Backup herunterladen und offline lagern

Zum späteren Ausgeben den WIF in eine volle VDS-Node importieren.
