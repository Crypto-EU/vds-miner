@echo off
setlocal
chcp 65001 >nul
set "ROOT=%~dp0"
cd /d "%ROOT%"

if not exist "%ROOT%vds-wallet.exe" (
  echo.
  echo  vds-wallet.exe fehlt in:
  echo  %ROOT%
  echo.
  echo  ZIP von GitHub nach C:\VDS-Wallet entpacken, oder Installieren.bat starten.
  echo.
  pause
  exit /b 1
)

echo.
echo  VDS-Wallet startet.
echo  Der Browser öffnet sich auf  http://127.0.0.1:43187
echo  Dieses Fenster offen lassen. Beenden: Fenster schließen oder Strg+C.
echo.
echo  Wenn Windows warnt: Weitere Informationen → Trotzdem ausführen.
echo.

"%ROOT%vds-wallet.exe"
if errorlevel 1 (
  echo.
  echo  Start fehlgeschlagen.
  pause
)
endlocal
