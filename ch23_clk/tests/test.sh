#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_clk"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_clk 2>/dev/null || true
  insmod virt_clk.ko
  rmmod virt_clk
  echo "[PASS] insmod virt_clk"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
