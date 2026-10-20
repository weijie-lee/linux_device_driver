#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_nfc"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_nfc 2>/dev/null || true
  insmod virt_nfc.ko
  rmmod virt_nfc
  echo "[PASS] insmod virt_nfc"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
