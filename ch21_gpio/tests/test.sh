#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_gpio"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_gpio 2>/dev/null || true
  insmod virt_gpio.ko
  rmmod virt_gpio
  echo "[PASS] insmod virt_gpio"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
