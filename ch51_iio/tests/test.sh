#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_iio"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_iio 2>/dev/null || true
  insmod virt_iio.ko
  rmmod virt_iio
  echo "[PASS] insmod virt_iio"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
