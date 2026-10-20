#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_mux"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_mux 2>/dev/null || true
  insmod virt_mux.ko
  rmmod virt_mux
  echo "[PASS] insmod virt_mux"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
