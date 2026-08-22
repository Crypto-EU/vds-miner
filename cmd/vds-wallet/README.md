# Portable VDS-Wallet

Kleine lokale HTTP-App: Oberfläche aus `web-wallet/`, Proxy zum Explorer [vdscool.com](https://www.vdscool.com/) und 666pool.

```bash
./scripts/build-wallet.sh
```

- Windows: `VDS-Wallet/vds-wallet.exe` (Doppelklick über `Start-VDS-Wallet.bat`)
- Linux: `bin/vds-wallet`
- ZIP: `dist/VDS-Wallet.zip`

```bash
VDS_WALLET_LISTEN=0.0.0.0:43187 ./bin/vds-wallet -open=false
```
