#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_dmaeng"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_dmaeng 2>/dev/null || true
  insmod virt_dmaeng.ko
  rmmod virt_dmaeng
  echo "[PASS] insmod virt_dmaeng"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
