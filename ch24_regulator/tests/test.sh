#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_regulator"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_regulator 2>/dev/null || true
  insmod virt_regulator.ko
  rmmod virt_regulator
  echo "[PASS] insmod virt_regulator"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
