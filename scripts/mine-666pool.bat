@echo off
REM Windows-Start gegen 666pool. Argument 1 = VDS-Wallet.
set WALLET=%1
set WORKER=%2
if "%WORKER%"=="" set WORKER=rig1
if "%WALLET%"=="" (
  echo Nutzung: mine-666pool.bat VDS_WALLET [worker]
  echo Pool: stratum+tcp://vds.666pool.com:9338
  exit /b 1
)
if not exist build\vds-miner.exe (
  echo Bitte zuerst unter Linux/WSL bauen. HiveOS und AMD-GPUs laufen unter Linux.
  exit /b 1
)
build\vds-miner.exe -o stratum+tcp://vds.666pool.com:9338 -u %WALLET%.%WORKER% -p x --api-port 4068
