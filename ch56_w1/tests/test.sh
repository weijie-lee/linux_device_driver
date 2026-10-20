#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_w1"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_w1 2>/dev/null || true
  insmod virt_w1.ko
  rmmod virt_w1
  echo "[PASS] insmod virt_w1"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
