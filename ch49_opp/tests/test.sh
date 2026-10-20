#!/bin/bash
set -e
cd "$(dirname "$0")/.."
make
echo "[PASS] build virt_opp"
if [ "$(uname -r)" = "5.15.0-173-generic" ]; then
  rmmod virt_opp 2>/dev/null || true
  insmod virt_opp.ko
  rmmod virt_opp
  echo "[PASS] insmod virt_opp"
else
  echo "[SKIP] insmod on $(uname -r); QEMU 5.15 covers load"
fi
