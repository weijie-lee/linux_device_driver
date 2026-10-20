#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_mtd"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_mtd 2>/dev/null || true
  insmod virt_mtd.ko
  rmmod virt_mtd
  echo "[PASS] insmod virt_mtd"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
