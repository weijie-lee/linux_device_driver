#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_tty"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_tty 2>/dev/null || true
  insmod virt_tty.ko
  rmmod virt_tty
  echo "[PASS] insmod virt_tty"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
