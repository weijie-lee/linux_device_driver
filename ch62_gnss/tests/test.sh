#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_gnss"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_gnss 2>/dev/null || true
  insmod virt_gnss.ko
  rmmod virt_gnss
  echo "[PASS] insmod virt_gnss"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
