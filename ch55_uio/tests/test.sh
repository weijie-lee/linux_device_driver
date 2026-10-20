#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_uio"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_uio 2>/dev/null || true
  insmod virt_uio.ko
  rmmod virt_uio
  echo "[PASS] insmod virt_uio"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
