#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_mfd"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_mfd 2>/dev/null || true
  insmod virt_mfd.ko
  rmmod virt_mfd
  echo "[PASS] insmod virt_mfd"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
