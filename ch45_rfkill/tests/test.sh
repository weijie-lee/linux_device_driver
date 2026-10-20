#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_rfkill"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_rfkill 2>/dev/null || true
  insmod virt_rfkill.ko
  rmmod virt_rfkill
  echo "[PASS] insmod virt_rfkill"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
