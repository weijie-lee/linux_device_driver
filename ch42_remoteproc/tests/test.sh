#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_rproc"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_rproc 2>/dev/null || true
  insmod virt_rproc.ko
  rmmod virt_rproc
  echo "[PASS] insmod virt_rproc"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
