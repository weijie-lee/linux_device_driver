#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_v4l2"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_v4l2 2>/dev/null || true
  insmod virt_v4l2.ko
  rmmod virt_v4l2
  echo "[PASS] insmod virt_v4l2"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
