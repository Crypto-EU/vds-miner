@echo off
setlocal
chcp 65001 >nul
set "ROOT=%~dp0"
cd /d "%ROOT%"

if not exist "%ROOT%core-qt.exe" (
  echo.
  echo  core-qt.exe fehlt in:
  echo  %ROOT%
  echo.
  echo  Ordner nach C:\VDS-Wallet kopieren, oder VDS-Wallet.zip dorthin entpacken.
  echo.
  pause
  exit /b 1
)

if not exist "%ROOT%data" mkdir "%ROOT%data"

echo.
echo  Originale VDS-Wallet startet.
echo  Extraordner:  %ROOT%
echo  Daten:        %ROOT%data
echo.
echo  Nach dem Start: Settings - Encrypt Wallet, dann Receive / getnewaddress
echo  Die Mining-Adresse muss mit Vc beginnen.
echo.

start "VDS-Wallet" "%ROOT%core-qt.exe" -datadir="%ROOT%data"
endlocal
