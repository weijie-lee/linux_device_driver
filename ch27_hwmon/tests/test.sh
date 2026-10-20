#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_hwmon"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_hwmon 2>/dev/null || true
  insmod virt_hwmon.ko
  rmmod virt_hwmon
  echo "[PASS] insmod virt_hwmon"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
