#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_i3c"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_i3c 2>/dev/null || true
  insmod virt_i3c.ko
  rmmod virt_i3c
  echo "[PASS] insmod virt_i3c"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
