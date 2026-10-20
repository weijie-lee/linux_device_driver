#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_nvmem"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_nvmem 2>/dev/null || true
  insmod virt_nvmem.ko
  rmmod virt_nvmem
  echo "[PASS] insmod virt_nvmem"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
