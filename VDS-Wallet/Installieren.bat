@echo off
setlocal
chcp 65001 >nul
set "DEST=C:\VDS-Wallet"
set "HERE=%~dp0"

echo.
echo  Kopiere VDS-Wallet nach:
echo  %DEST%
echo.

if not exist "%DEST%" mkdir "%DEST%"

if exist "%HERE%vds-wallet.exe" (
  copy /y "%HERE%vds-wallet.exe" "%DEST%\" >nul
  copy /y "%HERE%Start-VDS-Wallet.bat" "%DEST%\" >nul
  copy /y "%HERE%Lies-mich.txt" "%DEST%\" >nul
  if exist "%HERE%README.md" copy /y "%HERE%README.md" "%DEST%\" >nul
) else (
  echo  vds-wallet.exe fehlt neben diesem Skript.
  echo  Bitte VDS-Wallet.zip von GitHub nach C:\VDS-Wallet entpacken.
  pause
  exit /b 1
)

echo  Fertig.
echo  %DEST%
echo.
explorer "%DEST%"
start "" "%DEST%\Start-VDS-Wallet.bat"
endlocal
