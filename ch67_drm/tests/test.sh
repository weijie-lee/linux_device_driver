#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_drm"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_drm 2>/dev/null || true
  insmod virt_drm.ko
  rmmod virt_drm
  echo "[PASS] insmod virt_drm"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
