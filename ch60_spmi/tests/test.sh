#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_spmi"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_spmi 2>/dev/null || true
  insmod virt_spmi.ko
  rmmod virt_spmi
  echo "[PASS] insmod virt_spmi"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
