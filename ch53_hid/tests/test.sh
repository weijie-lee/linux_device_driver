#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_hid"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_hid 2>/dev/null || true
  insmod virt_hid.ko
  rmmod virt_hid
  echo "[PASS] insmod virt_hid"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
