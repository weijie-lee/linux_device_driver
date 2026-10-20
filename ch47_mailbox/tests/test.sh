#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_mbox"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_mbox 2>/dev/null || true
  insmod virt_mbox.ko
  rmmod virt_mbox
  echo "[PASS] insmod virt_mbox"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
