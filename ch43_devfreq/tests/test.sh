#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_devfreq"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_devfreq 2>/dev/null || true
  insmod virt_devfreq.ko
  rmmod virt_devfreq
  echo "[PASS] insmod virt_devfreq"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
