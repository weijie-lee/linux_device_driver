#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_phy"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_phy 2>/dev/null || true
  insmod virt_phy.ko
  rmmod virt_phy
  echo "[PASS] insmod virt_phy"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
