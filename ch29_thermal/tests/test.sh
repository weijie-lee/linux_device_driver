#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_thermal"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_thermal 2>/dev/null || true
  insmod virt_thermal.ko
  rmmod virt_thermal
  echo "[PASS] insmod virt_thermal"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
