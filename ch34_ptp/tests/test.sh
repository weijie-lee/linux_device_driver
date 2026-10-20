#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_ptp"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_ptp 2>/dev/null || true
  insmod virt_ptp.ko
  rmmod virt_ptp
  echo "[PASS] insmod virt_ptp"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
