#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_led"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_led 2>/dev/null || true
  insmod virt_led.ko
  rmmod virt_led
  echo "[PASS] insmod virt_led"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
