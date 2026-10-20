#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_pps"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_pps 2>/dev/null || true
  insmod virt_pps.ko
  rmmod virt_pps
  echo "[PASS] insmod virt_pps"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
