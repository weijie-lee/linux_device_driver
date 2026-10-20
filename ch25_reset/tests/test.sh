#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_reset"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_reset 2>/dev/null || true
  insmod virt_reset.ko
  rmmod virt_reset
  echo "[PASS] insmod virt_reset"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
