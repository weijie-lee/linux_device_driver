#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_alsa"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_alsa 2>/dev/null || true
  insmod virt_alsa.ko
  rmmod virt_alsa
  echo "[PASS] insmod virt_alsa"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
