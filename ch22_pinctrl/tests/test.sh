#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_pinctrl"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_pinctrl 2>/dev/null || true
  insmod virt_pinctrl.ko
  rmmod virt_pinctrl
  echo "[PASS] insmod virt_pinctrl"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
