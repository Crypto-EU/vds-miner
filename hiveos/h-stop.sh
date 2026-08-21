#!/usr/bin/env bash
# Optional HiveOS stop hook
pkill -f '/hive/miners/custom/vds-miner/vds-miner' 2>/dev/null || true
