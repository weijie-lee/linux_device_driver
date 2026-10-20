#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_fb"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_fb 2>/dev/null || true
  insmod virt_fb.ko
  rmmod virt_fb
  echo "[PASS] insmod virt_fb"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
