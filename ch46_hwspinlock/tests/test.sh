#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_hwspin"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_hwspin 2>/dev/null || true
  insmod virt_hwspin.ko
  rmmod virt_hwspin
  echo "[PASS] insmod virt_hwspin"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
