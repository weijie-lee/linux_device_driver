#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_fpga"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_fpga 2>/dev/null || true
  insmod virt_fpga.ko
  rmmod virt_fpga
  echo "[PASS] insmod virt_fpga"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
