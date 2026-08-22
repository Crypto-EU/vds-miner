@echo off
setlocal
chcp 65001 >nul
REM Installiert die originale VDS-Wallet nach C:\VDS-Wallet (Extraordner).
set "DEST=C:\VDS-Wallet"
set "HERE=%~dp0"

echo.
echo  Installiere originale VDS-Wallet nach:
echo  %DEST%
echo.

if not exist "%DEST%" mkdir "%DEST%"

if exist "%HERE%core-qt.exe" (
  xcopy /e /i /y "%HERE%*" "%DEST%\" >nul
) else if exist "%HERE%..\wallet\Wallet.zip" (
  powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "Expand-Archive -Force '%HERE%..\wallet\Wallet.zip' '%TEMP%\vds-wallet-src';" ^
    "Copy-Item -Force -Recurse '%TEMP%\vds-wallet-src\wallet\*' '%DEST%\'"
  copy /y "%HERE%Start-VDS-Wallet.bat" "%DEST%\" >nul
  copy /y "%HERE%Lies-mich.txt" "%DEST%\" >nul
) else (
  echo  Keine Wallet-Dateien gefunden.
  echo  Bitte VDS-Wallet.zip von GitHub nach C:\VDS-Wallet entpacken.
  pause
  exit /b 1
)

if not exist "%DEST%\data" mkdir "%DEST%\data"
if not exist "%DEST%\Start-VDS-Wallet.bat" copy /y "%HERE%Start-VDS-Wallet.bat" "%DEST%\" >nul

echo  Fertig. Ordner:
echo  %DEST%
echo.
explorer "%DEST%"
start "" "%DEST%\Start-VDS-Wallet.bat"
endlocal
