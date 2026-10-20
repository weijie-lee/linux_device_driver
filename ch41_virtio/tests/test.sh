#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_virtio"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_virtio 2>/dev/null || true
  insmod virt_virtio.ko
  rmmod virt_virtio
  echo "[PASS] insmod virt_virtio"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
