#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_icc"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_icc 2>/dev/null || true
  insmod virt_icc.ko
  rmmod virt_icc
  echo "[PASS] insmod virt_icc"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
