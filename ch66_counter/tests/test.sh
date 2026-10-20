#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_counter"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_counter 2>/dev/null || true
  insmod virt_counter.ko
  rmmod virt_counter
  echo "[PASS] insmod virt_counter"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
