#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_powercap"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_powercap 2>/dev/null || true
  insmod virt_powercap.ko
  rmmod virt_powercap
  echo "[PASS] insmod virt_powercap"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
