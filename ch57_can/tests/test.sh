#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_can"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_can 2>/dev/null || true
  insmod virt_can.ko
  rmmod virt_can
  echo "[PASS] insmod virt_can"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
